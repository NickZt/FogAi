import time
import json
import psutil
from transformers import pipeline, AutoTokenizer, AutoModelForCausalLM
import torch

def get_ram_mb():
    process = psutil.Process()
    return process.memory_info().rss / 1024 / 1024

def test_llm():
    print("=== Testing Heavy LLM Extraction (Qwen2.5-0.5B-Instruct) ===")
    start_ram = get_ram_mb()
    
    # Load model
    model_name = "Qwen/Qwen2.5-0.5B-Instruct"
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    model = AutoModelForCausalLM.from_pretrained(
        model_name,
        torch_dtype=torch.float16,
        device_map="auto"
    )
    
    load_ram = get_ram_mb()
    print(f"RAM Usage Baseline -> Model Loaded: {load_ram - start_ram:.2f} MB")

    text = "The quick brown fox jumps over the lazy dog in New York at 5 PM on Monday."
    
    prompt = f"""
Extract all entities from the following text and categorize them by ['animal', 'location', 'time', 'date'].
Output ONLY valid JSON.
Text: "{text}"
"""

    inputs = tokenizer(prompt, return_tensors="pt").to(model.device)
    input_tokens = inputs.input_ids.shape[1]
    print(f"Input Prompt Tokens: {input_tokens}")

    # Time Inference
    start_time = time.time()
    outputs = model.generate(**inputs, max_new_tokens=100)
    end_time = time.time()
    
    output_tokens = outputs.shape[1] - input_tokens
    print(f"Generated Tokens: {output_tokens}")

    response = tokenizer.decode(outputs[0][inputs.input_ids.shape[1]:], skip_special_tokens=True)
    
    print(f"Inference Time: {(end_time - start_time) * 1000:.2f} ms")
    print(f"Tokens per second: {output_tokens / (end_time - start_time):.2f}")
    print("Generation Output:")
    print(response)

if __name__ == "__main__":
    test_llm()
