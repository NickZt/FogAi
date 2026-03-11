import requests
import time
import json

TEXT = "The quick brown fox jumps over the lazy dog in New York at 5 PM on Monday."
LABELS = ["animal", "location", "time", "date"]

def test_node(model_id, node_name):
    print(f"\n=== Testing {node_name} ({model_id}) ===")
    url = "http://localhost:8080/v1/chat/completions"
    headers = {"Content-Type": "application/json"}
    
    # We send a completion request formatted for Extraction layer if that's how it's exposed, 
    # or perhaps MNNLLama has an embeddings/extraction endpoint.
    # In MNNLLama, GLiNER is usually called via specific prompt format or just chat completions with system prompt.
    payload = {
        "model": model_id,
        "messages": [{"role": "user", "content": json.dumps({"text": TEXT, "labels": LABELS})}]
    }

    start = time.time()
    try:
        response = requests.post(url, headers=headers, json=payload, timeout=10)
        end = time.time()
        print(f"Status Code: {response.status_code}")
        if response.status_code == 200:
            print(f"Inference Time: {(end - start) * 1000:.2f} ms")
            print(f"Response: {response.json()}")
        else:
            print(f"Error: {response.text}")
    except Exception as e:
        print(f"Connection Error: {e}")

if __name__ == "__main__":
    test_node("native-gliner-bi-v2", "Node A (MNN JNI local)")
    test_node("onnx-gliner-bi-v2", "Node B (ONNX gRPC Remote)")
