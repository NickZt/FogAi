# FogAI Architecture Overview

## 1. Vision & Objectives
**FogAI** is a high-performance, distributed **fog/edge-first platform for AI inference**. It exposes an OpenAI-compatible API while leveraging a heterogeneous multi-engine backend (MNN, ONNX Runtime, TensorRT) to execute models across diverse hardware architectures (ARM64, x86_64, GPU/NPU).

**Key Use Case:** Intelligent Edge / Fog Computing. Real-time processing of data streams (DSA - Decision Support Agents) where low latency, energy efficiency, and optimization for edge hardware are critical.

## 2. Core Architecture
The system follows a hybrid architecture combining high-performance in-process inference with scalable microservices.

```mermaid
graph TD
    Client[Clients / Edge Devices] -->|HTTP/JSON| Gateway[API Gateway - Vert.x]
    Gateway --> Router[Inference Router & Orchestrator]
    
    subgraph "Fog Node - Local"
        Router -->|JNI / Zero-Copy| EngineJNI[In-Process Engine<br/>MNN/ORT]
        EngineJNI -.-|DirectBuffer| Memory[Shared Memory / NPU]
    end
    
    subgraph "Cluster / Cloud"
        Router -->|gRPC / HTTP2| EngineRemote[Out-of-Process Service]
        EngineRemote --> TensorRT[TensorRT / vLLM]
    end
    
    style Client fill:#e1f5ff
    style Gateway fill:#fff4e6
    style Router fill:#fff4e6
    style EngineJNI fill:#e8f5e9
    style Memory fill:#e8f5e9
    style EngineRemote fill:#fce4ec
    style TensorRT fill:#fce4ec
```

## 3. Inference Node Types
FogAI defines two distinct types of inference nodes to balance **real-time performance** and **scalability**:

### Type A: In-Process JNI Nodes
- **Implementation:** Native C++ libraries (MNN/ONNX Runtime) loaded directly into the JVM process (Vert.x).
- **Data Path:** Vert.x off-heap `Buffer` → `nioBuffer()` → `DirectByteBuffer` → JNI → Native Tensor.
- **Latency:** Microsecond-level overhead (~20–50 µs).
- **Zero-Copy:** Data payload is passed by reference (pointer), avoiding intermediate copies.
- **Fail Strategy:** Tight coupling; a crash in native code affects the JVM.
- **Use Case:**
    - **CRITICAL** priority requests (DSA, Sensor Fusion).
    - Strict SLA requirements (0–10 ms).
    - Local Fog co-located models.

### Type B: Out-of-Process gRPC Nodes
- **Implementation:** Standalone C++ services communicating via gRPC (Protobuf).
- **Data Path:** Gateway → Serialization → Network/Localhost → Deserialization → Engine.
- **Latency:** Millisecond-level overhead (3–5 ms).
- **Scalability:** Processes can be scaled independently, distributed across hosts (Edge -> Fog -> Cloud).
- **Isolation:** Service crash does not impact the API Gateway.
- **Use Case:**
    - **Heavier Models** (LLMs > 50MB, GPU inference).
    - **NORMAL / BACKGROUND** priority traffic.
    - Multi-host deployments.

## 4. Routing & Orchestration Strategy
The **Inference Router** acts as the intelligent brain of the node.

### Selection Policy
| Priority | Model Size | Location | Action |
| :--- | :--- | :--- | :--- |
| **CRITICAL** | Small/Med | Local | **Use JNI Node** (Preempt others) |
| **NORMAL** | Any | Local/Remote | **Use gRPC Node** (Load Balance) |
| **BACKGROUND** | Any | Any | **Use Queue** (Process when idle) |

### Real-Time Prioritization (DSA Ready)
Unlike standard FIFO queues, FogAI implements **Multi-Level Priority Queues** with **Earliest Deadline First (EDF)** scheduling:
1.  **CRITICAL Queue:** For low-latency sensor events. Can preempt NORMAL tasks if supported by engine.
2.  **NORMAL Queue:** Standard user interactions (Chat, completions).
3.  **BACKGROUND Queue:** Analytics, batch jobs.

## 5. Technology Stack
- **Backend:** Kotlin / Java 21 (Vert.x) - Non-blocking I/O event loop.
- **Inference Engines:**
    - **MNN (Alibaba):** Primary engine for ARM/Mobile/Edge (CPU/NPU/OpenCL).
    - **ONNX Runtime:** Universal engine for varying hardware.
    - **TensorRT:** High-throughput GPU inference.
- **Interoperability:** JNI / Project Panama (Java 21+) for high-performance native access.
- **Protocol:** gRPC (Internal), HTTP/SSE (External API).
- **Database:** MongoDB (Metadata, Logs, Vector Store).

## 6. Zero-Copy Pipeline
To maximize efficiency on Edge/ARM devices, the data flow minimizes memory copies:

```mermaid
flowchart LR
    subgraph "HTTP Layer"
        A[HTTP Request Body] -->|1| B[Netty Off-Heap Buffer]
    end
    
    subgraph "JNI Bridge"
        B -->|2. nioBuffer| C[DirectByteBuffer]
        C -->|3. GetDirectBufferAddress| D[Native Pointer]
    end
    
    subgraph "Engine Layer"
        D -->|4. Tensor::create| E[MNN Tensor View<br/>Zero-Copy]
        E -->|5. Inference| F[Output Tensor]
        F -->|6. Wrap| G[Output DirectBuffer]
    end
    
    subgraph "Response"
        G -->|7. Netty Buffer| H[HTTP Response]
    end
    
    style B fill:#e8f5e9
    style C fill:#e8f5e9
    style D fill:#e8f5e9
    style E fill:#c8e6c9
    style F fill:#c8e6c9
    style G fill:#e8f5e9
```

**Pipeline Steps:**
1.  HTTP Request body is read into Netty's off-heap memory.
2.  Pointer to this memory is passed via JNI to the Inference Engine.
3.  Engine constructs a Tensor view over this memory (no copy).
4.  Inference runs in-place or writes to a pre-allocated output buffer.
5.  Output buffer is wrapped in a Netty Buffer and sent as HTTP Response.

## 7. Deployment Architecture

```mermaid
graph TB
    subgraph "Edge Device - ARM64"
        EdgeGW[Gateway<br/>:8080]
        EdgeMNN[MNN JNI<br/>In-Process]
        EdgeGW -.->|JNI| EdgeMNN
    end
    
    subgraph "Fog Node - x86_64"
        FogGW[Gateway<br/>:8080]
        FogMNN[MNN Service<br/>:50051]
        FogONNX[ONNX Service<br/>:50052]
        
        FogGW -->|gRPC| FogMNN
        FogGW -->|gRPC| FogONNX
    end
    
    subgraph "Cloud - GPU Cluster"
        CloudLB[Load Balancer]
        CloudGW1[Gateway 1]
        CloudGW2[Gateway 2]
        TRT1[TensorRT Service<br/>GPU 1]
        TRT2[TensorRT Service<br/>GPU 2]
        
        CloudLB --> CloudGW1
        CloudLB --> CloudGW2
        CloudGW1 --> TRT1
        CloudGW2 --> TRT2
    end
    
    EdgeDevice[IoT Sensors] -->|Local| EdgeGW
    EdgeGW -->|Offload| FogGW
    FogGW -->|Heavy Workload| CloudLB
    
    style EdgeGW fill:#a5d6a7
    style EdgeMNN fill:#81c784
    style FogGW fill:#fff59d
    style FogMNN fill:#fdd835
    style CloudLB fill:#ef9a9a
    style TRT1 fill:#e57373
```

## 8. Component Interactions

```mermaid
sequenceDiagram
    participant C as Client
    participant H as HTTP Handler
    participant R as Router
    participant S as Service Registry
    participant J as JNI Engine
    participant G as gRPC Client
    participant M as MNN Service
    
    C->>H: POST /v1/chat/completions
    H->>R: ChatRequest
    R->>S: Lookup Model
    S-->>R: Service Configuration
    
    alt Local JNI Available
        R->>J: inferenceGenerate(buffer)
        J-->>R: tokens
    else Remote gRPC
        R->>G: ChatCompletion(request)
        G->>M: gRPC Stream
        loop Streaming Response
            M-->>G: ChatResponse
            G-->>R: Delta
            R-->>H: SSE Event
            H-->>C: data: {...}
        end
    end
    
    R-->>H: Complete
    H-->>C: [DONE]
```

## 9. Model Discovery & Registration

```mermaid
flowchart TD
    Start[Gateway Startup] --> LoadConfig[Load nodes.json]
    LoadConfig --> ScanLocal[Scan MNN_MODELS_DIR]
    
    ScanLocal --> LocalModels[Register Local Models<br/>Prefix: native-]
    
    LoadConfig --> QueryRemote[Query Remote Services<br/>gRPC ListModels]
    QueryRemote --> RemoteModels[Register Remote Models<br/>Prefix: mnngrpc, onnx-]
    
    LocalModels --> Registry[Model Registry]
    RemoteModels --> Registry
    
    Registry --> Ready[Gateway Ready]
    
    Client[Client Request] --> Match{Model ID Match?}
    Match -->|Exact| Route1[Route to Service]
    Match -->|Prefix| Route2[Prefix-based Routing]
    Match -->|None| Error[404 Model Not Found]
    
    Route1 --> InferenceService[Inference Service]
    Route2 --> InferenceService
    
    style Registry fill:#fff59d
    style Ready fill:#a5d6a7
    style Error fill:#ef9a9a
```

## 10. Future Roadmap
- **NativeMnnBridge:** Implement the JNI/Panama layer for MNN.
- **Model Registry:** Dynamic discovery and management of models.
- [x] DSA Logic / Priority Queue Implementation in Vert.x
- [ ] TensorRT GPU Support
- **Distributed Tracing:** OpenTelemetry integration for observability.
- **Request Batching:** Optimize throughput for batch inference.
- **Model Warm-up:** Pre-load frequently used models.
- **Adaptive Routing:** ML-based routing decisions based on load and latency.

## 11. Performance Characteristics

| Component | Metric | Target | Actual |
|-----------|--------|--------|--------|
| **JNI Overhead** | Latency | <50µs | 20-50µs |
| **gRPC Overhead** | Latency | <5ms | 3-5ms |
| **Gateway Throughput** | Requests/sec | >1000 | ~800-1200 |
| **MNN Inference** | Tokens/sec | >50 | 40-80 (model dependent) |
| **Memory Footprint** | Gateway | <512MB | ~300MB |
| **Memory Footprint** | MNN Service | <2GB | 500MB-2GB (model dependent) |

## 12. Security Considerations

- **Authentication:** Not implemented (to be added for production).
- **Authorization:** Model-level access control (planned).
- **Rate Limiting:** Request throttling (planned).
- **Input Validation:** Sanitize user inputs to prevent injection attacks.
- **Model Isolation:** Each service runs in isolated process/container.
- **Secure Communication:** TLS for gRPC (recommended for production).
