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

# 8. Priority Queue Preemption
print_header "TEST CASE 8: Priority Queue Preemption (EDF)"
echo "Spawning 30 BACKGROUND embedding tasks..."
for i in {1..30}; do
  curl -s -X POST "$GATEWAY_URL/v1/embeddings" \
    -H "Content-Type: application/json" \
    -d '{"model": "'"$EMBEDDING_MODEL"'", "input": "Load generator text payload '$i' here."}' > /dev/null &
done

echo "Immediately sending 1 CRITICAL gliner-bi-v2 request..."
START_TIME=$(date +%s%3N)
curl -s -o /dev/null -w "Response Code: %{http_code}, Time: %{time_total}s\n" -X POST "$GATEWAY_URL/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "onnx-gliner-bi-v2",
    "messages": [{"role": "user", "content": "Extract entities from this text."}],
    "stream": false
  }'
END_TIME=$(date +%s%3N)
DURATION=$((END_TIME - START_TIME))
echo "Critical Request Client-Side Duration: ${DURATION}ms"
echo "Expected: Should complete quickly and preempt background queue."

# 9. Memory Restraint Swapping
print_header "TEST CASE 9: Memory Constraint Swapping"
echo "Alternating between $ONNX_MODEL and gliner-bi-v2 to trigger LRU eviction..."
for i in {1..4}; do
  if [ $((i%2)) -eq 0 ]; then
     TARGET="$ONNX_MODEL"
  else
     TARGET="onnx-gliner-bi-v2"
  fi
  echo "Requesting $TARGET ..."
  curl -s -o /dev/null -w "Status: %{http_code}\n" -X POST "$GATEWAY_URL/v1/chat/completions" \
    -H "Content-Type: application/json" \
    -d '{
      "model": "'"$TARGET"'",
      "messages": [{"role": "user", "content": "Hello."}],
      "stream": false
    }'
done
echo "Expected: Engine successfully swaps models without returning 500 or crashing out of memory."

echo ""
echo "=== Test Suite Complete ==="
