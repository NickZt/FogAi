#!/bin/bash

# verify_concurrency.sh
# This script sends a streaming chat completion request and 10 concurrent embedding requests
# to the API gateway to verify memory isolation under the new Micro-Verticle architecture.

# Terminal colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "Starting Concurrency Test: Embeddings + Chat Stream"

# Start the stream in the background
echo "1. Initiating Chat Completion Stream (Background)..."
curl -s -v -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "native-qwen2.5-7b",
    "messages": [
      {
        "role": "user",
        "content": "Write a long, detailed story about the history of the MNN inference engine, focusing on edge capabilities and mobile architecture. Ensure it is very comprehensive."
      }
    ],
    "stream": true,
    "maxTokens": 2048
  }' > chat_stream_output.log &
  
CHAT_PID=$!
echo "Chat Stream PID: $CHAT_PID"

echo "Waiting for 2 seconds to let the stream generate tokens..."
sleep 2

# Fire the comprehensive Python backpressure and health-check testsuite
echo "2. Firing Strict Multi-Queue Backpressure Testsuite..."

# Determine python executable (prefer venv if exists)
PYTHON_CMD="python3"
if [ -f "../venv/bin/python" ]; then
    PYTHON_CMD="../venv/bin/python"
fi

$PYTHON_CMD ../testsuite/test_concurrency_and_health.py

echo "Waiting for background Chat Stream process to settle..."
wait

echo -e "${GREEN}Concurrency test completed.${NC}"
echo "Check the native logs on the Gateway terminal to ensure no Segment Faults or OOM errors occurred."
echo "Chat output has been saved to chat_stream_output.log."
