#!/bin/bash
set -e

# Model Directory
MODEL_DIR="/home/nickzt/Projects/LLM_campf/models_mnn"
mkdir -p "$MODEL_DIR"

# Target Model: Qwen2.5-Omni-7B-MNN
MODEL_NAME="Qwen2.5-Omni-7B-MNN"
TARGET_DIR="$MODEL_DIR/$MODEL_NAME"

echo "Downloading $MODEL_NAME to $TARGET_DIR..."

if [ -d "$TARGET_DIR" ]; then
    echo "Directory $TARGET_DIR already exists. Skipping Qwen download."
else
    # Use git clone from ModelScope (reliable for large files if git-lfs is installed)
    # Ensuring git-lfs is initialized
    git lfs install --skip-smudge || true

    echo "Cloning from ModelScope..."
    git clone https://www.modelscope.cn/MNN/Qwen2.5-Omni-7B-MNN.git "$TARGET_DIR"
fi

echo "Download complete!"
echo "Model location: $TARGET_DIR"

# Praetor AI GLiNER Model
ONNX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/models_onnx"
GLINER_DIR="$ONNX_DIR/gliner-bi-v2"
mkdir -p "$ONNX_DIR"

echo "Checking GLiNER model..."
if [ ! -d "$GLINER_DIR" ]; then
    echo "Downloading pre-compiled ONNX GLiNER-bi-v2..."
    huggingface-cli download Deepchecks/gliner-bi-v2.1-onnx --local-dir "$GLINER_DIR"
else
    echo "GLiNER ONNX model already exists at $GLINER_DIR."
fi
