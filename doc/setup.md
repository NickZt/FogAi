# Setup Guide

This guide walks you through setting up the FogAI platform from scratch.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Environment Setup](#environment-setup)
- [Building the Gateway](#building-the-gateway)
- [Building MNN Service](#building-mnn-service)
- [Building ONNX Service](#building-onnx-service)
- [Model Preparation](#model-preparation)
- [Configuration](#configuration)
- [Running Services](#running-services)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **OS** | Linux (Ubuntu 20.04+) | Linux (Ubuntu 22.04+) or macOS |
| **CPU** | 4 cores | 8+ cores, AVX512 support |
| **RAM** | 8GB | 16GB+ |
| **Storage** | 10GB | 50GB+ (for models) |
| **GPU** | None | NVIDIA GPU with CUDA 11+ (optional) |

### Software Dependencies

#### Gateway (Kotlin/Java)
- **Java**: OpenJDK 21 or later
- **Gradle**: 8.x (included via wrapper)

```bash
# Install Java 21
sudo apt update
sudo apt install openjdk-21-jdk

# Verify installation
java -version
```

#### Inference Services (C++)
- **CMake**: 3.20 or later
- **Conan**: 2.0 or later
- **C++ Compiler**: GCC 9+, Clang 12+, or MSVC 2019+
- **Git**: For submodules

```bash
# Install build tools (Ubuntu)
sudo apt install build-essential cmake git

# Install Conan
pip install conan

# Verify installations
cmake --version
conan --version
```

#### Optional
- **Python**: 3.8+ (for test scripts)
- **CUDA Toolkit**: 11.0+ (for GPU support)

## Environment Setup

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/FogAI.git
cd FogAI
```

### 2. Initialize Submodules

The MNN library is included as a Git submodule:

```bash
git submodule update --init --recursive
```

This will clone the Alibaba MNN framework into `inference-services/mnn-service/MNN/`.

### 3. Setup Environment Variables

Create `.env` files for each component:

#### Gateway `.env`
```bash
cd gateway
cp .env.example .env
```

Edit `gateway/.env`:
```properties
GATEWAY_PORT=8080
MNN_MODELS_DIR=/home/user/FogAI/models
MONGO_URI=mongodb://localhost:27017/fogai
LOG_LEVEL=INFO
```

#### MNN Service `.env`
```bash
cd inference-services/mnn-service
```

Create `inference-services/mnn-service/.env`:
```properties
GRPC_PORT=50051
MODEL_PATH=/home/user/FogAI/models
THREAD_COUNT=4
AVX512_ENABLED=true
LOG_LEVEL=INFO
```

## Building the Gateway

### 1. Navigate to Gateway Directory

```bash
cd gateway
```

### 2. Build with Gradle

```bash
./gradlew build
```

This will:
- Download dependencies
- Compile Kotlin sources
- Generate gRPC client stubs from `proto/inference.proto`
- Run unit tests
- Create executable JAR in `build/libs/`

### 3. Verify Build

```bash
./gradlew test
```

Expected output:
```
BUILD SUCCESSFUL in 30s
```

## Building MNN Service

### 1. Navigate to MNN Service Directory

```bash
cd inference-services/mnn-service
```

### 2. Install Dependencies with Conan

```bash
# Create default Conan profile (first time only)
conan profile detect --force

# Install dependencies
conan install . --build=missing --settings=build_type=Release
```

This installs:
- gRPC
- Protobuf
- Abseil
- Other dependencies

### 3. Build MNN Library

MNN is built as part of the CMake configuration:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
```

This will:
- Configure CMake with Conan toolchain
- Build MNN library with optimizations (AVX512, AMX)
- Generate gRPC service stubs
- Configure include paths

### 4. Build MNN Service

```bash
cmake --build build --config Release
```

Expected output:
```
[100%] Built target mnn_service
```

### 5. Verify Build

```bash
ls -lh build/
```

You should see:
- `mnn-service` - Executable
- `libMNN.so` (or `.dylib` on macOS) - MNN library

## Building ONNX Service

### 1. Navigate to ONNX Service Directory

```bash
cd inference-services/onnx-service
```

### 2. Install Dependencies

```bash
conan install . --build=missing --settings=build_type=Release
```

### 3. Build ONNX Service

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build build --config Release
```

## Model Preparation

### 1. Create Model Directory

```bash
mkdir -p ~/FogAI/models
```

### 2. Download Models

#### MNN Models
MNN models require conversion from ONNX/PyTorch:

```bash
# Example: Convert Qwen2.5-0.5B to MNN format
# (This is model-specific, refer to MNN documentation)
cd inference-services/mnn-service/MNN/tools/converter

# Convert using MNN converter
./MNNConvert -f ONNX --modelFile qwen2.5-0.5b.onnx --MNNModel qwen2.5-0.5b.mnn
```

Place converted models in:
```
models/
├── qwen2-0.5b-instruct/
│   ├── qwen2-0.5b.mnn
│   ├── config.json
│   └── tokenizer.model
└── Qwen3-Embedding-0.6B-MNN/
    ├── qwen3-embed.mnn
    └── config.json
```

#### ONNX Models
For ONNX Runtime, download pre-converted ONNX models:

```bash
# Example: Qwen2.5-0.5B-Instruct-ONNX
cd ~/FogAI/models_onnx
wget https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-ONNX/resolve/main/model.onnx
```

### 3. Model Directory Structure

Expected structure:
```
models/
├── qwen2-0.5b-instruct/
│   ├── qwen.mnn
│   ├── qwen_config.json
│   └── tokenizer.txt
├── Qwen3-Embedding-0.6B-MNN/
│   └── embedding.mnn
└── ...
```

## Configuration

### 1. Configure Service Registry

Edit `gateway/nodes.json`:

```json
{
  "nodes": [
    {
      "id": "local-mnn",
      "type": "mnn-jni",
      "prefix": "native-",
      "enabled": false,
      "comment": "In-process JNI (not yet implemented)"
    },
    {
      "id": "remote-mnn",
      "type": "grpc",
      "host": "localhost",
      "port": 50051,
      "prefix": "mnngrpc",
      "enabled": true
    },
    {
      "id": "onnx-service",
      "type": "grpc",
      "host": "localhost",
      "port": 50052,
      "prefix": "onnx-",
      "enabled": true
    }
  ]
}
```

### 2. Configure Logging

For detailed logging, edit `gateway/src/main/resources/logback.xml`:

```xml
<configuration>
    <appender name="STDOUT" class="ch.qos.logback.core.ConsoleAppender">
        <encoder>
            <pattern>%d{HH:mm:ss.SSS} [%thread] %-5level %logger{36} - %msg%n</pattern>
        </encoder>
    </appender>
    
    <logger name="com.tactorder" level="DEBUG"/>
    
    <root level="INFO">
        <appender-ref ref="STDOUT" />
    </root>
</configuration>
```

## Running Services

### 1. Start MNN Service

```bash
cd inference-services/mnn-service
./build/mnn-service
```

Expected output:
```
[INFO] MNN Inference Service starting...
[INFO] Listening on 0.0.0.0:50051
[INFO] Available models:
[INFO]   - qwen2-0.5b-instruct
[INFO]   - Qwen3-Embedding-0.6B-MNN
[INFO] Service ready
```

### 2. Start ONNX Service (Optional)

In a new terminal:
```bash
cd inference-services/onnx-service
./build/onnx-service --port 50052
```

### 3. Start Gateway

In a new terminal:
```bash
cd gateway
./gradlew run
```

Expected output:
```
[INFO] API Gateway starting on port 8080
[INFO] Registering services from nodes.json...
[INFO] Discovered 2 remote models from remote-mnn
[INFO] Gateway ready at http://localhost:8080
```

### 4. Verify Services

```bash
# Check models endpoint
curl http://localhost:8080/v1/models

# Test chat completion
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "mnngrpcqwen2-0.5b-instruct",
    "messages": [{"role": "user", "content": "Hello!"}],
    "stream": false,
    "max_tokens": 20
  }'
```

## Troubleshooting

### Gateway Issues

#### Port Already in Use
```bash
# Check what's using port 8080
lsof -i :8080

# Kill the process
kill -9 <PID>

# Or change GATEWAY_PORT in .env
```

#### Models Not Found
```bash
# Verify MNN_MODELS_DIR is correct
echo $MNN_MODELS_DIR

# Check directory exists and has models
ls -la $MNN_MODELS_DIR
```

### MNN Service Issues

#### gRPC Build Errors
```bash
# Clean Conan cache
conan remove "*" --confirm

# Reinstall dependencies
conan install . --build=missing
```

#### MNN Compilation Errors
```bash
# Clean build
rm -rf build CMakeCache.txt

# Rebuild MNN submodule
cd MNN
git clean -fdx
cd ..

# Reconfigure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build build --config Release
```

#### Performance Issues
```bash
# Enable AVX512 (if supported)
echo "AVX512_ENABLED=true" >> .env

# Increase thread count
echo "THREAD_COUNT=8" >> .env
```

### Model Loading Errors

#### Invalid Model Format
```
Error: Failed to load model: invalid MNN format
```

**Solution**: Re-convert model using MNN converter with correct settings.

#### Tokenizer Not Found
```
Error: Could not find tokenizer.txt
```

**Solution**: Ensure tokenizer file is in the same directory as model file.

### Network Issues

#### Cannot Connect to gRPC Service
```bash
# Check service is running
ps aux | grep mnn_service

# Check port is listening
netstat -tuln | grep 50051

# Test gRPC directly
grpcurl -plaintext localhost:50051 list
```

## Next Steps

- [API Reference](api-reference.md) - Detailed API documentation
- [Development Guide](development.md) - Contributing to the project
- [Deployment Guide](deployment.md) - Production deployment strategies

## Getting Help

- **GitHub Issues**: https://github.com/yourusername/FogAI/issues
- **Documentation**: [doc/](../doc/)
- **MNN Documentation**: https://mnn-docs.readthedocs.io/
