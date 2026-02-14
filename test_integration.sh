#!/bin/bash

# Configuration
GATEWAY_URL="http://localhost:8080"
ONNX_MODEL="onnx-Qwen2.5-0.5B-Instruct-ONNX"
JNI_MODEL="native-qwen2-0.5b-instruct"
EMBEDDING_MODEL="native-Qwen3-Embedding-0.6B-MNN"

echo "=== TactOrder Gateway Integration Test Suite ==="
echo "Target: $GATEWAY_URL"
echo "Timestamp: $(date)"
echo "------------------------------------------------"

# Helper function for separator
print_header() {
    echo ""
    echo ">>> $1"
    echo "------------------------------------------------"
}

# 1. List Models
print_header "TEST CASE 1: List Models (GET /v1/models)"
echo "Expected: JSON list containing '$ONNX_MODEL', '$JNI_MODEL', and '$EMBEDDING_MODEL'"
curl -s -X GET "$GATEWAY_URL/v1/models" | grep -oE "($ONNX_MODEL|$JNI_MODEL|$EMBEDDING_MODEL)" | sort | uniq
echo ""

# 2. ONNX Chat Completion (Stream=True)
print_header "TEST CASE 2: ONNX Chat Completion (Stream=True)"
echo "Model: $ONNX_MODEL"
curl -s -N -X POST "$GATEWAY_URL/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "'"$ONNX_MODEL"'",
    "messages": [
      {"role": "user", "content": "Explain quantum entanglement briefly."}
    ],
    "stream": true,
    "max_tokens": 50
  }'
echo ""

# 3. ONNX Chat Completion (Stream=False)
print_header "TEST CASE 3: ONNX Chat Completion (Stream=False)"
echo "Model: $ONNX_MODEL"
# Note: Gateway currently might not aggregate stream into single response nicely for all backends, 
# but let's test what it returns. If it fails, that's a finding.
curl -s -X POST "$GATEWAY_URL/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "'"$ONNX_MODEL"'",
    "messages": [
      {"role": "user", "content": "What is 2+2?"}
    ],
    "stream": false
  }'
echo ""

# 4. JNI Chat Completion (Stream=True)
print_header "TEST CASE 4: JNI Chat Completion (Stream=True)"
echo "Model: $JNI_MODEL"
curl -s -N -X POST "$GATEWAY_URL/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "'"$JNI_MODEL"'",
    "messages": [
      {"role": "user", "content": "Write a haiku about code."}
    ],
    "stream": true,
    "max_tokens": 50
  }'
echo ""

# 5. JNI Chat Completion (Stream=False)
print_header "TEST CASE 5: JNI Chat Completion (Stream=False)"
echo "Model: $JNI_MODEL"
curl -s -X POST "$GATEWAY_URL/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "'"$JNI_MODEL"'",
    "messages": [
      {"role": "user", "content": "Say hello only."}
    ],
    "stream": false
  }'
echo ""

# 6. Embeddings (JNI)
print_header "TEST CASE 6: Embeddings Generation (JNI)"
echo "Model: $EMBEDDING_MODEL"
# Assuming /v1/embeddings endpoint
curl -s -X POST "$GATEWAY_URL/v1/embeddings" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "'"$EMBEDDING_MODEL"'",
    "input": "The quick brown fox jumps over the lazy dog"
  }' | cut -c 1-100 
echo "... (truncated)"

# 7. Error Handling: Invalid Model
print_header "TEST CASE 7: Error Handling (Invalid Model)"
echo "Model: invalid-model-id"
curl -s -X POST "$GATEWAY_URL/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "invalid-model-id",
    "messages": [{"role": "user", "content": "test"}]
  }'
echo ""

echo ""
echo "=== Test Suite Complete ==="
