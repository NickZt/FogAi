
import grpc
import sys
import os
sys.path.append(os.getcwd())

import inference_pb2
import inference_pb2_grpc

def run_client():
    print("Connecting to ONNX Service at localhost:50052...")
    channel = grpc.insecure_channel('localhost:50052')
    stub = inference_pb2_grpc.InferenceServiceStub(channel)

    print("Sending ListModels request...")
    request = inference_pb2.ListModelsRequest()
    
    try:
        response = stub.ListModels(request)
        print("ListModels response:")
        for model in response.models:
            print(f" - {model.id}")
            
    except grpc.RpcError as e:
        print(f"RPC Error: {e}")

if __name__ == '__main__':
    run_client()
