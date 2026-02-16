# MNN Inference Engine Service

## Overview
This service wraps the Alibaba MNN framework to provide high-performance inference on edge devices (ARM, x86). It exposes a gRPC interface for the Gateway and integrates MNN LLM for text generation.

## Tech Stack
- **Language:** C++17
- **Frameworks:**
    - [MNN](https://github.com/alibaba/MNN) (Inference Engine) - **Git Submodule** at `inference-services/mnn-service/MNN`
    - [gRPC](https://grpc.io/) (Communication) - Managed via **Conan**
    - [nlohmann/json](https://github.com/nlohmann/json) (Configuration) - **Header Only**

## Interface (gRPC)
See `inference.proto` for exact definitions. The service implements:
- `ChatCompletion(ChatRequest) returns (stream ChatResponse)`
- `Embedding(EmbeddingRequest) returns (EmbeddingResponse)`
- `ListModels(ListModelsRequest) returns (ListModelsResponse)`

## Key Features
- **Session Management:** Reuse MNN Interpreters/Sessions to avoid reloading models.
- **Single Active Model:** Unloads previous model before loading new one to manage memory.
- **Observability:** Logs performance metrics (Prefill Time, Decode Time, Speed) to stdout.
- **Optimization:** AVX512/AMX enabled for high performance on modern CPUs.

## Build System
- **MNN**: Built as a nested submodule (part of the project source).
- **Dependencies**: `gRPC`, `Protobuf`, `Abseil` are provided by **Conan**.
- **CMake**: `cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake` handles dependency injection.

## Implementation Status
- **Build System:** CMake + Conan (gRPC/Protobuf) + Submodule (MNN).
- **Service:** Fully functional `InferenceServiceImpl`.
- **Inference:** MNN LLM integrated (supports Qwen2.5 models).
- **Port:** Listens on `0.0.0.0:50051`.
- **Metrics:** Logs tokens/sec and latency.

## Tokenization
- Handled internally by MNN LLM (uses embedded tokenizer in model directory).
