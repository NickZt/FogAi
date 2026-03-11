# API Gateway & Orchestrator

## Overview
The Gateway is the central component that exposes the OpenAI-compatible API and orchestrates inference requests to backend services. It follows a Clean Architecture pattern to decouple business logic from infrastructure concerns.

## Tech Stack
- **Language:** Kotlin
- **Framework:** Eclipse Vert.x (Web, gRPC Client)
- **Build System:** Gradle
- **Architecture:** Clean Architecture (Domain, Application, Infrastructure)

## Public API (OpenAI Compatible)
The Gateway implements the following endpoints fully compatible with OpenAI API specs.

### Endpoints

| Method | Path | Description | Status |
| :--- | :--- | :--- | :--- |
| POST | `/v1/chat/completions` | Chat completions (streaming & non-streaming) | Implemented |
| POST | `/v1/embeddings` | Vector embeddings | Implemented |
| GET | `/v1/models` | List available models | Implemented |

## Internal Architecture

### Clean Architecture Layers
1.  **Domain Layer**: Core business logic and interfaces (`InferenceService`, `ChatRequest`, `ChatResponse`).
2.  **Application Layer**: Orchestrates use cases (`ChatService`).
3.  **Infrastructure Layer**: Implements interfaces (`MnnJniService`, `GrpcInferenceService`).
4.  **API Layer**: Handles HTTP requests (`ChatCompletionHandler`, `EmbeddingsHandler`, `ModelsHandler`).

### Router (`RouterAi`)
Decides which service handles a request.
- **Service Registry**: Loads from `nodes.json`.
- **Dynamic Discovery**:
    - **Local**: Scans `MNN_MODELS_DIR` and registers models with a configured prefix (e.g., `native-`).
    - **Remote**: Registers gRPC clients and queries their model lists, applying a prefix (e.g., `mnngrpc`).
- **Routing Strategy**:
    1.  **Exact Match**: Routes to service with registered model ID.
    2.  **Prefix Match**: Fallback routing based on model ID prefix (e.g., `mnngrpc*`).

### gRPC Client (`InferenceClient`)
Maintains connections to remote Inference Services.
- **Proto**: `inference.proto`.
- **Streaming**: Handles bi-directional streaming for chat completions.

## Configuration
Configuration is managed via `.env` files and `nodes.json`.

- **`.env`**: Global settings.
    - `GATEWAY_PORT`: Port for external API (default: 8080).
    - `MNN_MODELS_DIR`: Directory for local MNN models.

- **`nodes.json`**: Service Registry & Queue Management.
    - Defines nodes (`local-mnn`, `remote-gpu`).
    - Configures connection details (host, port).
    - Assigns **prefixes** (`native-`, `mnngrpc`, `onnx-`) for routing and model discovery.
    - **`queue` section**: Defines Earliest Deadline First (EDF) SLA limits (`criticalSlaMs`, `normalSlaMs`, `backgroundSlaMs`) to restrict memory exhaustion from overlapping traffic.

Example `nodes.json`:
```json
{
  "queue": {
    "criticalSlaMs": 50,
    "normalSlaMs": 5000,
    "backgroundSlaMs": 60000,
    "maxQueueSize": 1000
  },
  "nodes": [
    {
      "id": "local-mnn",
      "type": "mnn-jni",
      "prefix": "native-"
    },
    {
      "id": "onnx-service",
      "type": "grpc",
      "host": "localhost",
      "port": 50052,
      "prefix": "onnx-"
    }
  ]
}
```
