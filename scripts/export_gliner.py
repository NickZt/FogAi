from optimum.onnxruntime import ORTModelForFeatureExtraction
from transformers import AutoTokenizer

model_id = "knowledgator/gliner-bi-base-v2.0"
save_dir = "models_onnx/gliner-bi-v2"

print(f"Downloading and exporting {model_id} to {save_dir}...")
# This automatically handles the ONNX export for unsupported architectures by falling back to standard tracing
model = ORTModelForFeatureExtraction.from_pretrained(model_id, export=True)
tokenizer = AutoTokenizer.from_pretrained(model_id)

model.save_pretrained(save_dir)
tokenizer.save_pretrained(save_dir)
print("ONNX Export complete.")
