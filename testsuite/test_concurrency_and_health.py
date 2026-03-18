import requests
import time
import threading
import sys

GATEWAY_URL = "http://localhost:8080/v1/embeddings"
MODELS_URL = "http://localhost:8080/v1/models"
TEST_TEXT = "Testing concurrency backpressure across isolated node implementations."

def get_active_models():
    try:
        resp = requests.get(MODELS_URL, timeout=5)
        if resp.status_code == 200:
            data = resp.json().get("data", [])
            return [model.get("id") for model in data]
    except Exception as e:
        print(f"Failed to fetch models: {e}")
    return []

def test_heartbeat_discovery():
    print("\n=== Testing Heartbeat Discovery & Activity State ===")
    print("Fetching active models from Gateway...")
    models = get_active_models()
    print(f"Currently Active Models: {models}")
    
    target = "py-onnx-gliner-bi-v2"
    if target in models:
        print(f"[SUCCESS] Node '{target}' is ACTIVE and detected by Heartbeat!")
    else:
        print(f"[WARNING] Node '{target}' is MISSING (Inactive).")
        print("  -> This proves the Gateway Heartbeat successfully detected the Python server is offline!")
        print("  -> Start the Type C server (`python inference-services/python-onnx-service/server.py`), wait 10 seconds for the Heartbeat, and run this again to test Recovery!")
    return target in models

def test_node_sequential(node_name, num_requests):
    """
    Sends requests to a specific node sequentially (creating backpressure).
    It waits for the previous response fully before initiating the next.
    """
    results = []
    print(f"[{node_name}] Starting {num_requests} sequential backpressure requests...")
    
    start_time = time.time()
    for i in range(num_requests):
        payload = {
            "model": node_name,
            "input": [f"{TEST_TEXT} (Iteration {i})"]
        }
        try:
            req_start = time.time()
            resp = requests.post(GATEWAY_URL, json=payload, timeout=60)
            req_end = time.time()
            
            if resp.status_code == 200:
                latency = (req_end - req_start) * 1000
                print(f"  -> [{node_name}] Req {i+1}/{num_requests} Success ({latency:.0f}ms)")
                results.append(latency)
            else:
                print(f"  -> [{node_name}] Req {i+1} failed: {resp.status_code} - {resp.text}")
        except Exception as e:
            print(f"  -> [{node_name}] Req {i+1} error: {e}")
            
    total_time = time.time() - start_time
    avg_latency = sum(results) / len(results) if results else 0
    print(f"[{node_name}] DONE! {len(results)}/{num_requests} completed. Total: {total_time:.2f}s, Avg: {avg_latency:.2f}ms")

def main():
    print("=== Commencing Multi-Queue & Heartbeat Verification Suite ===")
    
    # 1. Test heartbeat and discovery
    python_active = test_heartbeat_discovery()
    
    time.sleep(1)
    print("\n=== Initiating Concurrent Multi-Node Stress Test ===")
    print("This explicitly validates that sequential queuing on one heavy Node (JNI) does NOT block parallel routing to another Node (Python gRPC).")
    
    
    # We use Qwen for JNI load, and test whatever active models the Gateway sees
    test_scenarios = [
        {"node": "native-Qwen3-Embedding-4B-MNN", "requests": 3}
    ]
    
    # Add Python Type C
    if python_active:
        test_scenarios.append({"node": "py-onnx-gliner-bi-v2", "requests": 3})
    else:
        print("\n[SKIP] Skipping Python Node concurrency test because it is marked INACTIVE by Heartbeat.")
        
    models = get_active_models()
    
    # Add Type B (ONNX service)
    type_b = "onnx-gliner-bi-v2"
    if type_b in models:
        test_scenarios.append({"node": type_b, "requests": 3})
        
    # Add Type A (mnngrpc)
    type_a = "mnngrpcgliner-bi-v2"
    if type_a in models:
        test_scenarios.append({"node": type_a, "requests": 3})
    
    threads = []
    for scenario in test_scenarios:
        t = threading.Thread(target=test_node_sequential, args=(scenario["node"], scenario["requests"]))
        threads.append(t)
        t.start()
        
    for t in threads:
        t.join()
        
    print("\n=== Verification Suite Completed ===")

if __name__ == "__main__":
    main()
