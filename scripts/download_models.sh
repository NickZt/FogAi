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
    echo "Directory $TARGET_DIR already exists. Skipping download."
    echo "If you want to re-download, please remove the directory first."
    exit 0
fi

# Use git clone from ModelScope (reliable for large files if git-lfs is installed)
# Ensuring git-lfs is initialized
git lfs install --skip-smudge || true

echo "Cloning from ModelScope..."
git clone https://www.modelscope.cn/MNN/Qwen2.5-Omni-7B-MNN.git "$TARGET_DIR"

echo "Download complete!"
echo "Model location: $TARGET_DIR"
