#!/bin/bash
set -e

# Configuration
GATEWAY_URL="http://127.0.0.1:8080"
CONCURRENCY=50
TOTAL_REQUESTS=300000 # Enough to sustain 500 RPS for 10 min

# Requirements Check
if ! command -v ab &> /dev/null; then
    echo "Error: Apache Bench (ab) is not installed."
    echo "Install with: sudo apt-get install -y apache2-utils"
    exit 1
fi

PAYLOAD_FILE=$(mktemp)
cat <<EOF > "$PAYLOAD_FILE"
{
  "model": "native-Qwen3-Embedding-0.6B-MNN",
  "input": "Load testing text payload for embeddings generator to simulate 500 RPS."
}
EOF

echo "=== MNNLLama Praetor AI Stress Test ==="
echo "Target: $GATEWAY_URL"
echo "Duration: ~10 minutes (Concurrency: $CONCURRENCY, Target Total: $TOTAL_REQUESTS)"
echo "Hitting API Gateway with background embeddings requests..."

# We expect to see 429s as the queue hits its max size, but NO 500s or crashes.
ab -n $TOTAL_REQUESTS -c $CONCURRENCY -p "$PAYLOAD_FILE" -T "application/json" "$GATEWAY_URL/v1/embeddings"

rm -f "$PAYLOAD_FILE"
echo "Stress test complete."
