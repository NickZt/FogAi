
import grpc
import sys
import os
sys.path.append(os.getcwd())
from grpc_tools import protoc

def generate_protos():
    print("Generating protos...")
    proto_include = "proto"
    proto_file = "proto/inference.proto"
    
    if not os.path.exists(proto_file):
             print(f"Error: Proto file not found at {proto_file}")
             sys.exit(1)

    command = [
        'grpc_tools.protoc',
        f'-I{proto_include}',
        '--python_out=.',
        '--grpc_python_out=.',
        proto_file
    ]
    if protoc.main(command) != 0:
        print("Error: Failed to generate protos")
        sys.exit(1)
    print("Protos generated.")

def run_client():
    # Import generated modules
    try:
        import inference_pb2
        import inference_pb2_grpc
    except ImportError:
        generate_protos()
        import inference_pb2
        import inference_pb2_grpc

    print("Connecting to ONNX Service at localhost:50052...")
    channel = grpc.insecure_channel('localhost:50052')
    stub = inference_pb2_grpc.InferenceServiceStub(channel)

    # Chat completion request
    print("Sending ChatCompletion request...")
    messages = [
        inference_pb2.Message(role="system", content="You are a helpful assistant."),
        inference_pb2.Message(role="user", content="Hello, who are you?")
    ]
    
    # Check what model ID to use. 
    # expected path: ONNX_MODELS_DIR/Qwen2.5-0.5B-Instruct-ONNX/tokenizer.txt
    
    model_id = "Qwen2.5-0.5B-Instruct-ONNX"
    
    request = inference_pb2.ChatRequest(
        model_id=model_id,
        messages=messages,
        max_tokens=50,
        temperature=0.7,
        stream=True
    )

    try:
        for response in stub.ChatCompletion(request):
            print(f"Received chunk: {response.choices[0].delta.content}", end='')
        print("\nStream finished.")
    except grpc.RpcError as e:
        print(f"RPC Error: {e}")

if __name__ == '__main__':
    run_client()
