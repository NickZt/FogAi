# Internal Inference Interface

## 1. Dual-Interface Strategy
FogAI uses two distinct interfaces for communicating with inference engines, chosen based on the request priority and node type defined in `doc/architecture.md`.

## 2. Type A: In-Process JNI Interface (Critical Path)
**Goal:** Zero-Copy, Ultra-Low Latency (< 50 µs overhead).
**Status:** Planned (NativeMnnBridge).

### Data Flow
1.  **Input:**
    - `DirectByteBuffer` (mapped to off-heap memory) containing input IDs / embeddings.
    - `long` pointer to the buffer address.
    - `int` size.
2.  **Execution (C++):**
    - MNN creates `Tensor::create` using the raw pointer (No `memcpy`).
    - Inference executes in-place or writes to output `DirectByteBuffer`.
3.  **Output:**
    - `DirectByteBuffer` containing logits / tokens.

### Java Interface (Proposed)
```java
public interface NativeEngine {
    // Zero-copy inference
    int generate(long inputBufferAddress, int activeLength, long outputBufferAddress);
    
    // Management
    boolean loadModel(String path);
    void unloadModel();
}
```

## 3. Type B: Out-of-Process gRPC Interface (Standard Path)
**Goal:** Scalability, Isolation, Network Distribution.
**Status:** Implemented (`mnn-service`, `proto/inference.proto`).

### Service Definition
`InferenceService` exposes methods via gRPC.

#### `ChatCompletion`
- **Type:** Server-streaming RPC.
- **Request:** `ChatRequest` (OpenAI-compatible).
- **Response:** Stream of `ChatResponse`.
- **Latency:** ~3-5ms overhead.

#### `Embeddings`
- **Type:** Unary RPC.
- **Request:** `EmbeddingRequest`.
- **Response:** `EmbeddingResponse`.

## 4. Data Structures (Common)
Both interfaces map to the same logical data structures:
- **ChatRequest:** `messages`, `temperature`, `top_p`.
- **ChatResponse:** `delta`, `finish_reason`.
