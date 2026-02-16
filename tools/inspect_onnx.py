
import onnxruntime as ort
import sys
import os

def inspect_model(model_path):
    if not os.path.exists(model_path):
        print(f"Error: File not found: {model_path}")
        return

    print(f"Loading model: {model_path}...")
    try:
        session = ort.InferenceSession(model_path)
    except Exception as e:
        print(f"Error loading session: {e}")
        return

    print("\n--- Inputs ---")
    for i, meta in enumerate(session.get_inputs()):
        print(f"Input {i}: Name='{meta.name}', Type='{meta.type}', Shape={meta.shape}")

    print("\n--- Outputs ---")
    for i, meta in enumerate(session.get_outputs()):
        print(f"Output {i}: Name='{meta.name}', Type='{meta.type}', Shape={meta.shape}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python inspect_onnx.py <path_to_onnx_model>")
        sys.exit(1)
    
    inspect_model(sys.argv[1])
