# Changelog

All notable changes to FogAI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- JNI/Panama bridge for in-process MNN inference, Shared Memory Management (Arena Allocation)
- Preemptive EDF Scheduler: Logic to pause/cancel running `BACKGROUND` tasks when `CRITICAL` DSA request arrives (Real-time safety).
- TensorRT GPU support
- Request batching for improved throughput
- Model caching and warm-up 
- Distributed tracing with OpenTelemetry
- Prometheus metrics integration
- Score-based Routing, Adaptive Routing, NodeSelectionStrategy
- Model Partitioning, check entropy confidence then move it to gRPC/Cloud 
- mTLS authentication for gRPC inter-node communication.
- Local vector store (embedded MongoDB/Milvus) caching for autonomous decision making during network partition.

## [0.1.0] - 2026-02-16

### Added
- Initial public release of FogAI platform
- OpenAI-compatible API Gateway built with Vert.x (Kotlin)
  - `/v1/models` endpoint for model discovery
  - `/v1/chat/completions` endpoint (streaming and non-streaming)
  - `/v1/embeddings` endpoint for vector generation
- MNN Inference Service (C++ gRPC)
  - Support for Qwen2.5 models
  - AVX512/AMX optimizations
  - Server-side streaming
  - Model session management
- ONNX Runtime Inference Service (C++ gRPC)
  - Cross-platform inference support
  - OpenAI-compatible chat completions
- Service discovery and routing
  - Prefix-based model routing (`remote-`, `onnx-`, `native-`)
  - Dynamic model registration from gRPC services
  - Local model scanning from filesystem
- Protobuf-based gRPC interface
  - `InferenceService` definition
  - Chat completion, embeddings, and model listing
- Build system configuration
  - Gradle for Gateway (Kotlin/Java)
  - CMake + Conan for inference services (C++)
  - Git submodule for MNN library
- Integration test suite
  - Multi-model testing
  - Streaming and non-streaming validation
  - Error handling verification
- Documentation
  - Comprehensive README with architecture diagrams
  - Architecture overview with mermaid diagrams
  - Setup guide
  - API reference
  - Contributing guidelines
  - Apache License 2.0

### Technical Details
- **Gateway**: Java 21, Vert.x 4.x, Kotlin 1.9
- **MNN Service**: C++17, MNN 2.x, gRPC 1.60+
- **ONNX Service**: C++17, ONNX Runtime, gRPC 1.60+
- **Protocol**: gRPC for internal communication, HTTP/SSE for external API

### Known Limitations
- No authentication/authorization (planned for v0.2.0)
- JNI bridge not yet implemented
- Priority queue scheduling not implemented
- No request batching
- Limited observability (basic logging only)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for information on how to contribute to this project.

---

**Legend:**
- `Added` - New features
- `Changed` - Changes in existing functionality
- `Deprecated` - Soon-to-be removed features
- `Removed` - Removed features
- `Fixed` - Bug fixes
- `Security` - Security-related changes
