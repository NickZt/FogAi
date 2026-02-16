#!/bin/bash

# Test ONNX UTF-8 Fix
echo "Testing ONNX UTF-8..."
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "onnx-Qwen2.5-0.5B-Instruct-ONNX",
    "messages": [
      {"role": "user", "content": "Зроби договір цпх"}
    ],
    "stream": true,
    "max_tokens": 50
  }'
echo ""

# Test MNN Metrics
echo "Testing MNN Metrics..."
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "native-qwen2-0.5b-instruct",
    "messages": [
      {"role": "user", "content": "Hello, how are you?"}
    ],
    "stream": true,
    "max_tokens": 50
  }'
echo ""
