#!/bin/bash
set -e

# Resolve project root automatically based on script location
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ONNX_MODEL="$PROJECT_ROOT/models_onnx/gliner-bi-v2/onnx/model.onnx"
MNN_DIR="$PROJECT_ROOT/models_mnn/gliner-bi-v2"
MNN_MODEL="$MNN_DIR/llm.mnn"

if [ ! -f "$ONNX_MODEL" ]; then
    echo "Error: ONNX model not found at $ONNX_MODEL"
    echo "Please ensure the model is downloaded first."
    exit 1
fi

mkdir -p "$MNN_DIR"

echo "Converting GLiNER ONNX to MNN..."
mnnconvert -f ONNX --modelFile "$ONNX_MODEL" --MNNModel "$MNN_MODEL" --bizCode MNN

# Copy the configuration files needed by the model
echo "Copying configuration files..."
cp "$PROJECT_ROOT/models_onnx/gliner-bi-v2/"*.json "$MNN_DIR/"
cp "$PROJECT_ROOT/models_onnx/gliner-bi-v2/"*.txt "$MNN_DIR/" 2>/dev/null || true
cp "$PROJECT_ROOT/models_onnx/gliner-bi-v2/README.md" "$MNN_DIR/" 2>/dev/null || true

# Synthesize the MNN-LLM compatible llm_config.json for the C++ NativeEngine
echo "Generating llm_config.json..."
cat << 'EOF' > "$MNN_DIR/llm_config.json"
{
    "hidden_size": 768,
    "layer_nums": 12,
    "attention_mask": "int"
}
EOF

echo "Conversion complete!"
echo "MNN Model saved to: $MNN_MODEL"
ls -lh "$MNN_MODEL"
