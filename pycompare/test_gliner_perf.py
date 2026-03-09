import time
import json
import psutil
import torch
from transformers import AutoModelForTokenClassification, AutoTokenizer
from gliner import GLiNER

def get_ram_mb():
    process = psutil.Process()
    return process.memory_info().rss / 1024 / 1024

def test_gliner():
    print("=== Testing GLiNER Bi-Encoder (194M) ===")
    start_ram = get_ram_mb()
    
    # Load model
    model = GLiNER.from_pretrained("knowledgator/gliner-bi-base-v2.0")
    
    load_ram = get_ram_mb()
    print(f"RAM Usage Baseline -> Model Loaded: {load_ram - start_ram:.2f} MB")

    text = "The quick brown fox jumps over the lazy dog in New York at 5 PM on Monday."
    labels = ["animal", "location", "time", "date"]

    # Token length approximation for GLiNER
    from transformers import AutoTokenizer
    tokenizer = AutoTokenizer.from_pretrained("knowledgator/gliner-bi-base-v2.0")
    input_tokens = len(tokenizer.tokenize(text)) + len(tokenizer.tokenize(" ".join(labels)))
    print(f"Approximate Input Tokens (Text + Labels): {input_tokens}")

    # Time Inference
    start_time = time.time()
    entities = model.predict_entities(text, labels, threshold=0.3)
    end_time = time.time()
    
    print(f"Inference Time: {(end_time - start_time) * 1000:.2f} ms")
    print("Extracted Entities:")
    for entity in entities:
        print(f" - {entity['text']} [{entity['label']}] (Score: {entity['score']:.2f})")
    print("\n")

if __name__ == "__main__":
    test_gliner()
