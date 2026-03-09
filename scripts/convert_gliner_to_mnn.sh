#!/bin/bash
set -e

ONNX_MODEL="models_onnx/gliner-bi-v2/onnx/model.onnx"
MNN_DIR="models_mnn/gliner-bi-v2"
MNN_MODEL="$MNN_DIR/model.mnn"

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
cp models_onnx/gliner-bi-v2/*.json "$MNN_DIR/"
cp models_onnx/gliner-bi-v2/*.txt "$MNN_DIR/" 2>/dev/null || true
cp models_onnx/gliner-bi-v2/README.md "$MNN_DIR/" 2>/dev/null || true

echo "Conversion complete!"
echo "MNN Model saved to: $MNN_MODEL"
ls -lh "$MNN_MODEL"
