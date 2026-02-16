# Contributing to FogAI

Thank you for your interest in contributing to FogAI! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Project Structure](#project-structure)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Submitting Changes](#submitting-changes)
- [Reporting Issues](#reporting-issues)
- [Feature Requests](#feature-requests)

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inspiring community for everyone. Please be respectful and constructive in your interactions.

### Expected Behavior

- Use welcoming and inclusive language
- Be respectful of differing viewpoints and experiences
- Gracefully accept constructive criticism
- Focus on what is best for the community
- Show empathy towards other community members

## Getting Started

### 1. Fork the Repository

```bash
# Fork via GitHub UI, then clone your fork
git clone https://github.com/YOUR_USERNAME/FogAI.git
cd FogAI
```

### 2. Set Up Development Environment

Follow the [Setup Guide](doc/setup.md) to configure your development environment.

### 3. Create a Branch

```bash
# Create a feature branch
git checkout -b feature/amazing-feature

# Or a bugfix branch
git checkout -b fix/issue-123
```

Branch naming conventions:
- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation updates
- `refactor/description` - Code refactoring
- `test/description` - Test additions/improvements

## Development Workflow

### 1. Make Your Changes

Edit the relevant files following our [coding standards](#coding-standards).

### 2. Test Your Changes

```bash
# Gateway tests
cd gateway
./gradlew test

# MNN Service tests
cd inference-services/mnn-service
./build/Release/mnn_service_test

# Integration tests
cd ../..
./test_integration.sh
```

### 3. Commit Your Changes

```bash
git add .
git commit -m "feat: add amazing feature"
```

Commit message format:
```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

**Example:**
```
feat(gateway): add request batching support

Implement request batching to improve throughput for
embedding generation. Batches up to 32 requests with
a 10ms timeout.

Closes #123
```

### 4. Push to Your Fork

```bash
git push origin feature/amazing-feature
```

### 5. Create a Pull Request

Go to the original repository and create a pull request from your fork.

## Project Structure

```
FogAI/
├── gateway/                    # Kotlin/Vert.x API Gateway
│   └── src/
│       └── main/kotlin/com/tactorder/
│           ├── api/            # HTTP Handlers (add new endpoints here)
│           ├── domain/         # Core business logic
│           ├── application/    # Use cases
│           └── infrastructure/ # gRPC clients, external services
│
├── inference-services/
│   ├── mnn-service/           # C++ gRPC service for MNN
│   │   ├── src/
│   │   │   ├── main.cpp       # Service entry point
│   │   │   └── service_impl.cpp # gRPC service implementation
│   │   └── MNN/               # Git submodule
│   │
│   ├── onnx-service/          # C++ gRPC service for ONNX Runtime
│   └── common/                # Shared protobuf definitions
│
├── proto/
│   └── inference.proto        # gRPC service definition
│
└── doc/                       # Documentation
```

### Key Files

| File | Purpose | When to Edit |
|------|---------|--------------|
| `proto/inference.proto` | gRPC API definition | Adding new RPC methods |
| `gateway/nodes.json` | Service registry | Adding new inference services |
| `gateway/src/main/kotlin/com/tactorder/api/RouterAi.kt` | Request routing logic | Changing routing strategy |
| `inference-services/mnn-service/src/service_impl.cpp` | MNN inference implementation | MNN-specific changes |

## Coding Standards

### Kotlin (Gateway)

Follow [Kotlin Coding Conventions](https://kotlinlang.org/docs/coding-conventions.html):

```kotlin
// Use meaningful names
class InferenceRouter(
    private val serviceRegistry: ServiceRegistry,
    private val vertx: Vertx
) {
    // Prefer immutability
    fun route(request: ChatRequest): Future<ChatResponse> {
        // Use explicit types for clarity
        val service: InferenceService = findService(request.model)
        return service.generate(request)
    }
}

// Use extension functions
fun Buffer.toDirectByteBuffer(): ByteBuffer {
    return this.byteBuf.nioBuffer()
}
```

**Key Points:**
- Use 4 spaces for indentation
- Prefer `val` over `var`
- Use meaningful variable names
- Add KDoc comments for public APIs
- Follow Clean Architecture principles

### C++ (Inference Services)

Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html):

```cpp
// Use modern C++17 features
class InferenceServiceImpl final : public InferenceService::Service {
public:
    // Use smart pointers
    explicit InferenceServiceImpl(std::unique_ptr<Engine> engine)
        : engine_(std::move(engine)) {}

    // Override with const correctness
    grpc::Status ChatCompletion(
        grpc::ServerContext* context,
        const ChatRequest* request,
        grpc::ServerWriter<ChatResponse>* writer) override {
        
        // Use RAII for resource management
        auto session = engine_->createSession();
        
        // Prefer auto for complex types
        auto result = session->generate(request->messages());
        
        return grpc::Status::OK;
    }

private:
    std::unique_ptr<Engine> engine_;
};
```

**Key Points:**
- Use smart pointers (`unique_ptr`, `shared_ptr`)
- Follow RAII principles
- Use `const` correctness
- Prefer `auto` for complex types
- Add Doxygen-style comments

### Protobuf

```protobuf
syntax = "proto3";

package tactorder.inference;

// Use descriptive names
message ChatRequest {
  // Add comments for all fields
  string model_id = 1;  // Model identifier
  repeated Message messages = 2;  // Conversation history
  float temperature = 3;  // Sampling temperature (0.0-2.0)
}
```

## Testing Guidelines

### Gateway Unit Tests

```kotlin
class InferenceRouterTest {
    @Test
    fun `should route to correct service based on model prefix`() {
        val router = InferenceRouter(registry, vertx)
        val request = ChatRequest(
            model = "remote-qwen2-0.5b",
            messages = listOf(Message("user", "hello"))
        )
        
        val service = router.findService(request.model)
        
        assertEquals("grpc-service", service.id)
    }
}
```

### Integration Tests

Add test cases to `test_integration.sh`:

```bash
# Test new endpoint
print_header "TEST CASE X: New Feature Test"
curl -X POST "$GATEWAY_URL/v1/new-endpoint" \
  -H "Content-Type: application/json" \
  -d '{"param": "value"}'
echo ""
```

### C++ Tests

```cpp
TEST(InferenceServiceTest, GeneratesTokens) {
    auto engine = std::make_unique<MockEngine>();
    InferenceServiceImpl service(std::move(engine));
    
    ChatRequest request;
    request.set_model_id("test-model");
    
    // Test implementation
    EXPECT_TRUE(service.isReady());
}
```

## Submitting Changes

### Pull Request Checklist

Before submitting a PR, ensure:

- [ ] Code follows coding standards
- [ ] All tests pass
- [ ] New features have tests
- [ ] Documentation is updated
- [ ] Commit messages follow conventions
- [ ] No sensitive data (API keys, credentials) in code
- [ ] Changes are focused and minimal

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring

## Testing
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Manual testing completed

## Checklist
- [ ] Code follows project style
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] No breaking changes (or documented)

## Related Issues
Closes #123
Related to #456
```

### Review Process

1. **Automated Checks**: CI/CD runs tests and linters
2. **Code Review**: Maintainers review your code
3. **Feedback**: Address review comments
4. **Approval**: Once approved, your PR will be merged

## Reporting Issues

### Bug Reports

Use the bug report template:

```markdown
**Describe the bug**
A clear description of the bug

**To Reproduce**
Steps to reproduce:
1. Send request to...
2. With parameters...
3. Observe error...

**Expected behavior**
What should happen

**Environment**
- OS: Ubuntu 22.04
- Java: 21
- FogAI version: 0.1.0

**Logs**
Relevant log output
```

### Good Bug Reports Include:

- Clear, descriptive title
- Minimal reproduction steps
- Expected vs. actual behavior
- Environment details
- Relevant logs/screenshots

## Feature Requests

### Feature Request Template

```markdown
**Problem Statement**
What problem does this feature solve?

**Proposed Solution**
How would you solve it?

**Alternatives**
Other approaches considered

**Additional Context**
Any other relevant information
```

## Areas for Contribution

### High Priority

- [ ] **JNI Bridge Implementation** - Critical path for in-process inference
- [ ] **Priority Queue System** - Multi-level scheduling
- [ ] **Model Caching** - Reduce model loading times
- [ ] **Request Batching** - Improve throughput
- [ ] **Metrics & Observability** - Prometheus/Grafana integration

### Documentation

- [ ] API usage tutorials
- [ ] Model conversion guides
- [ ] Performance tuning guides
- [ ] Deployment examples (Docker, K8s)

### Testing

- [ ] Additional unit tests
- [ ] Load testing framework
- [ ] End-to-end tests
- [ ] Performance benchmarks

### Infrastructure

- [ ] CI/CD improvements
- [ ] Docker images
- [ ] Kubernetes manifests
- [ ] Terraform scripts

## Development Tips

### Local Testing

```bash
# Run gateway in dev mode with hot reload
cd gateway
./gradlew run --continuous

# Verbose logging for MNN service
cd inference-services/mnn-service
./build/Release/mnn_service --log-level=DEBUG

# Test a single endpoint
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d @test_chat.json
```

### Debugging

**Gateway (Kotlin):**
```kotlin
// Add debug logging
logger.debug("Routing request for model: ${request.model}")

// Use Vert.x debugging
vertx.setDeploymentOptions(
    DeploymentOptions().setWorkerPoolSize(10)
)
```

**MNN Service (C++):**
```cpp
// Add verbose logging
std::cout << "[DEBUG] Processing request for model: " 
          << request->model_id() << std::endl;

// Use gRPC tracing
setenv("GRPC_TRACE", "all", 1);
setenv("GRPC_VERBOSITY", "DEBUG", 1);
```

### Performance Profiling

```bash
# Profile Gateway (JVM)
./gradlew run -Dcom.sun.management.jmxremote

# Profile MNN Service (Linux perf)
perf record -g ./build/Release/mnn_service
perf report
```

## Getting Help

- **Documentation**: Check [doc/](doc/) folder first
- **GitHub Issues**: Search existing issues
- **Discussions**: Use GitHub Discussions for questions
- **Code Review**: Ask for clarification in PR comments

## Recognition

Contributors are recognized in:
- GitHub contributors page
- Release notes
- CONTRIBUTORS.md file

Thank you for contributing to FogAI!
