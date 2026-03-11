# API Reference

Complete reference for the FogAI OpenAI-compatible API.

## Base URL

```
http://localhost:8080/v1
```

## Authentication

Currently, the API does not require authentication. For production deployments, implement API key authentication.

## Common Headers

```http
Content-Type: application/json
Accept: application/json
```

## Endpoints

### List Models

Retrieve a list of available models.

```http
GET /v1/models
```

#### Response

```json
{
  "object": "list",
  "data": [
    {
      "id": "mnngrpcqwen2-0.5b-instruct",
      "object": "model",
      "created": 1708456320,
      "owned_by": "tactorder"
    },
    {
      "id": "native-Qwen3-Embedding-0.6B-MNN",
      "object": "model",
      "created": 1708456320,
      "owned_by": "tactorder"
    }
  ]
}
```

#### Model ID Prefixes

| Prefix | Service Type | Description |
|--------|--------------|-------------|
| `mnngrpc` | gRPC MNN Service | Out-of-process MNN inference |
| `onnx-` | gRPC ONNX Service | Out-of-process ONNX Runtime |
| `native-` | JNI | In-process JNI (planned) |

---

### Chat Completions

Create a chat completion (streaming or non-streaming).

```http
POST /v1/chat/completions
```

#### Request Body

```json
{
  "model": "mnngrpcqwen2-0.5b-instruct",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant."
    },
    {
      "role": "user",
      "content": "Explain quantum computing in simple terms."
    }
  ],
  "temperature": 0.7,
  "top_p": 0.9,
  "max_tokens": 150,
  "stream": true,
  "stop": ["</s>", "Human:"]
}
```

#### Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `model` | string | **Yes** | - | Model ID from `/v1/models` |
| `messages` | array | **Yes** | - | Array of message objects |
| `temperature` | number | No | 0.7 | Sampling temperature (0.0-2.0) |
| `top_p` | number | No | 1.0 | Nucleus sampling probability |
| `max_tokens` | integer | No | 100 | Maximum tokens to generate |
| `stream` | boolean | No | false | Enable streaming responses |
| `stop` | array | No | [] | Stop sequences |

#### Message Object

```json
{
  "role": "user|assistant|system",
  "content": "Message text"
}
```

#### Response (Non-Streaming)

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1708456320,
  "model": "mnngrpcqwen2-0.5b-instruct",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Quantum computing uses quantum mechanics..."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 20,
    "completion_tokens": 50,
    "total_tokens": 70
  }
}
```

#### Response (Streaming)

Server-Sent Events (SSE) format:

```
data: {"id":"chatcmpl-abc123","object":"chat.completion.chunk","created":1708456320,"model":"mnngrpcqwen2-0.5b-instruct","choices":[{"index":0,"delta":{"role":"assistant","content":"Quantum"},"finish_reason":null}]}

data: {"id":"chatcmpl-abc123","object":"chat.completion.chunk","created":1708456320,"model":"mnngrpcqwen2-0.5b-instruct","choices":[{"index":0,"delta":{"content":" computing"},"finish_reason":null}]}

data: {"id":"chatcmpl-abc123","object":"chat.completion.chunk","created":1708456320,"model":"mnngrpcqwen2-0.5b-instruct","choices":[{"index":0,"delta":{"content":" uses"},"finish_reason":null}]}

data: {"id":"chatcmpl-abc123","object":"chat.completion.chunk","created":1708456320,"model":"mnngrpcqwen2-0.5b-instruct","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]
```

#### Finish Reasons

| Reason | Description |
|--------|-------------|
| `stop` | Natural completion or stop sequence hit |
| `length` | Max tokens reached |
| `error` | Generation error occurred |

---

### Embeddings

Generate vector embeddings for input text.

```http
POST /v1/embeddings
```

#### Request Body

```json
{
  "model": "native-Qwen3-Embedding-0.6B-MNN",
  "input": "The quick brown fox jumps over the lazy dog"
}
```

Or batch processing:

```json
{
  "model": "native-Qwen3-Embedding-0.6B-MNN",
  "input": [
    "First sentence to embed",
    "Second sentence to embed",
    "Third sentence to embed"
  ]
}
```

#### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `model` | string | **Yes** | Embedding model ID |
| `input` | string or array | **Yes** | Text(s) to embed |

#### Response

```json
{
  "object": "list",
  "data": [
    {
      "object": "embedding",
      "index": 0,
      "embedding": [
        0.0023, -0.0091, 0.0045, ..., 0.0012
      ]
    }
  ],
  "model": "native-Qwen3-Embedding-0.6B-MNN",
  "usage": {
    "prompt_tokens": 8,
    "total_tokens": 8
  }
}
```

#### Embedding Dimensions

| Model | Dimensions |
|-------|------------|
| Qwen3-Embedding-0.6B | 768 |

---

## Error Handling

### Error Response Format

```json
{
  "error": {
    "message": "Model 'invalid-model' not found",
    "type": "invalid_request_error",
    "code": "model_not_found"
  }
}
```

### HTTP Status Codes

| Code | Meaning | Description |
|------|---------|-------------|
| 200 | OK | Request successful |
| 400 | Bad Request | Invalid request parameters |
| 404 | Not Found | Model or endpoint not found |
| 500 | Internal Server Error | Server-side error |
| 503 | Service Unavailable | Inference service unavailable |

### Common Error Types

#### Model Not Found

**Request:**
```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model": "invalid-model", "messages": [{"role": "user", "content": "test"}]}'
```

**Response:**
```json
{
  "error": {
    "message": "Model 'invalid-model' not found. Available models: mnngrpcqwen2-0.5b-instruct, native-Qwen3-Embedding-0.6B-MNN",
    "type": "invalid_request_error",
    "code": "model_not_found"
  }
}
```

#### Invalid Parameters

**Request:**
```json
{
  "model": "mnngrpcqwen2-0.5b-instruct",
  "messages": "invalid format"
}
```

**Response:**
```json
{
  "error": {
    "message": "'messages' must be an array",
    "type": "invalid_request_error",
    "code": "invalid_parameter"
  }
}
```

#### Service Unavailable

When the backend inference service is down:

```json
{
  "error": {
    "message": "Failed to connect to inference service at localhost:50051",
    "type": "service_error",
    "code": "service_unavailable"
  }
}
```

---

## Usage Examples

### cURL

#### List Models
```bash
curl http://localhost:8080/v1/models
```

#### Chat Completion (Non-Streaming)
```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "mnngrpcqwen2-0.5b-instruct",
    "messages": [
      {"role": "system", "content": "You are a helpful AI assistant."},
      {"role": "user", "content": "What is the capital of France?"}
    ],
    "temperature": 0.7,
    "max_tokens": 50
  }'
```

#### Chat Completion (Streaming)
```bash
curl -N -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "mnngrpcqwen2-0.5b-instruct",
    "messages": [{"role": "user", "content": "Write a haiku about coding"}],
    "stream": true,
    "max_tokens": 50
  }'
```

#### Generate Embeddings
```bash
curl -X POST http://localhost:8080/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{
    "model": "native-Qwen3-Embedding-0.6B-MNN",
    "input": "Embeddings are vector representations of text"
  }'
```

### Python

```python
import requests
import json

BASE_URL = "http://localhost:8080/v1"

# List models
response = requests.get(f"{BASE_URL}/models")
models = response.json()
print(f"Available models: {[m['id'] for m in models['data']]}")

# Chat completion (non-streaming)
response = requests.post(
    f"{BASE_URL}/chat/completions",
    json={
        "model": "mnngrpcqwen2-0.5b-instruct",
        "messages": [
            {"role": "user", "content": "Explain AI in one sentence"}
        ],
        "max_tokens": 50
    }
)
result = response.json()
print(f"Response: {result['choices'][0]['message']['content']}")

# Chat completion (streaming)
response = requests.post(
    f"{BASE_URL}/chat/completions",
    json={
        "model": "mnngrpcqwen2-0.5b-instruct",
        "messages": [{"role": "user", "content": "Count to 5"}],
        "stream": True,
        "max_tokens": 30
    },
    stream=True
)

for line in response.iter_lines():
    if line:
        decoded = line.decode('utf-8')
        if decoded.startswith('data: '):
            data = decoded[6:]
            if data == '[DONE]':
                break
            chunk = json.loads(data)
            content = chunk['choices'][0]['delta'].get('content', '')
            if content:
                print(content, end='', flush=True)

# Generate embeddings
response = requests.post(
    f"{BASE_URL}/embeddings",
    json={
        "model": "native-Qwen3-Embedding-0.6B-MNN",
        "input": "Sample text for embedding"
    }
)
embedding = response.json()['data'][0]['embedding']
print(f"Embedding dimension: {len(embedding)}")
```

### JavaScript/TypeScript

```typescript
const BASE_URL = "http://localhost:8080/v1";

// List models
async function listModels() {
  const response = await fetch(`${BASE_URL}/models`);
  const data = await response.json();
  console.log("Models:", data.data.map(m => m.id));
}

// Chat completion (non-streaming)
async function chatCompletion() {
  const response = await fetch(`${BASE_URL}/chat/completions`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "mnngrpcqwen2-0.5b-instruct",
      messages: [
        { role: "user", content: "What is 2+2?" }
      ],
      max_tokens: 20
    })
  });
  
  const result = await response.json();
  console.log("Response:", result.choices[0].message.content);
}

// Chat completion (streaming)
async function chatCompletionStream() {
  const response = await fetch(`${BASE_URL}/chat/completions`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "mnngrpcqwen2-0.5b-instruct",
      messages: [{ role: "user", content: "Write a poem" }],
      stream: true,
      max_tokens: 50
    })
  });

  const reader = response.body.getReader();
  const decoder = new TextDecoder();

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    
    const chunk = decoder.decode(value);
    const lines = chunk.split('\n').filter(line => line.trim());
    
    for (const line of lines) {
      if (line.startsWith('data: ')) {
        const data = line.slice(6);
        if (data === '[DONE]') return;
        
        const parsed = JSON.parse(data);
        const content = parsed.choices[0].delta.content || '';
        process.stdout.write(content);
      }
    }
  }
}

// Generate embeddings
async function generateEmbedding() {
  const response = await fetch(`${BASE_URL}/embeddings`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "native-Qwen3-Embedding-0.6B-MNN",
      input: "Text to embed"
    })
  });
  
  const result = await response.json();
  console.log("Embedding:", result.data[0].embedding.slice(0, 5), "...");
}
```

---

## Rate Limiting

Currently not implemented. For production, configure rate limiting based on:
- Requests per minute per client
- Tokens generated per hour
- Model-specific quotas

---

## WebSocket Support

**Status**: Not currently supported. All streaming uses Server-Sent Events (SSE).

**Planned**: WebSocket support for bidirectional communication in future releases.

---

## OpenAI Compatibility

FogAI is designed to be a drop-in replacement for OpenAI API clients. Most libraries that support OpenAI should work with minimal configuration changes:

```python
# Using OpenAI Python library
import openai

openai.api_base = "http://localhost:8080/v1"
openai.api_key = "not-needed"  # No auth currently

response = openai.ChatCompletion.create(
    model="mnngrpcqwen2-0.5b-instruct",
    messages=[{"role": "user", "content": "Hello!"}]
)
```

### Compatibility Notes

| Feature | OpenAI | FogAI | Notes |
|---------|--------|----------|-------|
| Chat Completions | ✅ | ✅ | Full support |
| Streaming | ✅ | ✅ | SSE format |
| Embeddings | ✅ | ✅ | Full support |
| Function Calling | ✅ | ❌ | Not yet supported |
| Image Input | ✅ | ❌ | Planned |
| Audio | ✅ | ❌ | Not planned |
| Fine-tuning | ✅ | ❌ | Not applicable |

---

## Performance Tips

1. **Use Streaming**: For long-form generation, enable `stream: true` for better UX
2. **Batch Embeddings**: Send multiple texts in a single embedding request
3. **Adjust max_tokens**: Set appropriate limits to reduce latency
4. **Model Selection**: Use smaller models for latency-critical applications
5. **Connection Pooling**: Reuse HTTP connections for better throughput

---

## See Also

- [Setup Guide](setup.md) - Installation and configuration
- [Architecture Overview](architecture.md) - System design
- [Development Guide](development.md) - Contributing and extending
