
import grpc
import sys
import os

# Add current directory to path to find pb2 files
sys.path.append(os.getcwd())

import inference_pb2
import inference_pb2_grpc

def run():
    print("Connecting to gRPC server at localhost:50052...")
    with grpc.insecure_channel('localhost:50052') as channel:
        stub = inference_pb2_grpc.InferenceServiceStub(channel)
        
        # 1. List Models
        print("\n--- Listing Models ---")
        try:
            response = stub.ListModels(inference_pb2.ListModelsRequest())
            model_id = ""
            for model in response.models:
                print(f"Found model: {model.id}")
                if "Qwen" in model.id or "onnx" in model.id:
                    model_id = model.id
            
            if not model_id and response.models:
                model_id = response.models[0].id
                
        except grpc.RpcError as e:
            print(f"ListModels failed: {e}")
            return

        if not model_id:
            print("No models found!")
            return

        print(f"\n--- Testing Chat Completion with model: {model_id} ---")
        # 2. Chat Completion
        req = inference_pb2.ChatRequest(
            model_id=model_id,
            messages=[
                inference_pb2.Message(role="user", content="Hello, thinking process test.")
            ],
            max_tokens=50,
            temperature=0.7
        )
        
        try:
            responses = stub.ChatCompletion(req)
            print("Streaming Response:")
            for resp in responses:
                for choice in resp.choices:
                    print(choice.delta.content, end="", flush=True)
            print("\n\n--- Done ---")
        except grpc.RpcError as e:
            print(f"ChatCompletion failed: {e}")

if __name__ == "__main__":
    run()
