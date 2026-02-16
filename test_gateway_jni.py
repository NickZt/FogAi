
import requests
import json
import sys

def test_jni_completion():
    url = "http://localhost:8080/v1/chat/completions"
    headers = {"Content-Type": "application/json"}
    # Use a local native model that exists in the logs
    data = {
        "model": "native-qwen2-0.5b-instruct", 
        "messages": [
            {"role": "user", "content": "Hello JNI!"}
        ],
        "stream": True
    }

    print(f"Sending request to {url} for model {data['model']}...")
    try:
        with requests.post(url, headers=headers, json=data, stream=True) as response:
            if response.status_code == 200:
                print("Response stream:")
                for line in response.iter_lines():
                    if line:
                        decoded_line = line.decode('utf-8')
                        if decoded_line.startswith("data: "):
                            if decoded_line == "data: [DONE]":
                                print("\n[DONE]")
                                break
                            json_str = decoded_line[6:] # Strip "data: "
                            try:
                                chunk = json.loads(json_str)
                                if "choices" in chunk and len(chunk["choices"]) > 0:
                                    delta = chunk["choices"][0].get("delta", {})
                                    content = delta.get("content", "")
                                    print(content, end="", flush=True)
                            except json.JSONDecodeError:
                                print(f"\nError decoding JSON: {json_str}")
            else:
                print(f"Error: {response.status_code}")
                print(response.text)
    except Exception as e:
        print(f"Request failed: {e}")

if __name__ == "__main__":
    test_jni_completion()
