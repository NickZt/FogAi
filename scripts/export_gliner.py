import torch
from transformers import AutoModel, AutoTokenizer
import os

model_id = "knowledgator/gliner-bi-base-v2.0"
print(f"Loading {model_id}...")

tokenizer = AutoTokenizer.from_pretrained(model_id)
model = AutoModel.from_pretrained(model_id, trust_remote_code=True)
model.eval()

# Dummy input
dummy_text = "The quick brown fox jumps over the lazy dog in New York at 5 PM on Monday."
inputs = tokenizer(dummy_text, padding="max_length", max_length=128, return_tensors="pt")

output_path = "models_onnx/gliner-bi-v2/model.pt"
os.makedirs(os.path.dirname(output_path), exist_ok=True)

print(f"Tracing PyTorch model to {output_path}...")
with torch.no_grad():
    traced_model = torch.jit.trace(model, (inputs['input_ids'], inputs['attention_mask']), strict=False)
    traced_model.save(output_path)

tokenizer.save_pretrained("models_onnx/gliner-bi-v2")
print("TorchScript trace complete.")
