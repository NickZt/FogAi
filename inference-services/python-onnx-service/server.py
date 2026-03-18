import sys
import os
import json
import time
from concurrent import futures
import grpc
import torch

# Append MNNLLama root to sys.path to access compiled Protobufs
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
if PROJECT_ROOT not in sys.path:
    sys.path.append(PROJECT_ROOT)

import inference_pb2
import inference_pb2_grpc
from gliner import GLiNER
from transformers import AutoModel, AutoTokenizer

class InferenceServiceServicer(inference_pb2_grpc.InferenceServiceServicer):
    def __init__(self):
        print("Loading GLiNER ONNX base model...")
        self.model = GLiNER.from_pretrained("knowledgator/gliner-bi-base-v2.0")
        self.model.eval()
        
        print("Loading underlying Transformer for pure embeddings...")
        self.base_encoder = AutoModel.from_pretrained("knowledgator/gliner-bi-base-v2.0")
        self.base_encoder.eval()
        self.tokenizer = AutoTokenizer.from_pretrained("knowledgator/gliner-bi-base-v2.0")
        print("GLiNER loaded via PyTorch/ONNX bridge.")

    def ListModels(self, request, context):
        res = inference_pb2.ListModelsResponse()
        card = res.models.add()
        card.id = "gliner-bi-v2"
        card.object = "model"
        card.created = int(time.time())
        card.owned_by = "python-onnx"
        return res

    def ChatCompletion(self, request, context):
        response = inference_pb2.ChatResponse()
        response.id = "chatcmpl-" + str(int(time.time()))
        response.model = request.model_id
        response.created = int(time.time())

        choice = response.choices.add()
        choice.index = 0

        try:
            prompt = request.messages[0].content
            try:
                data = json.loads(prompt)
                text = data.get("text", "")
                labels = data.get("labels", [])
            except json.JSONDecodeError:
                text = prompt
                labels = ["person", "location", "date"]

            entities = self.model.predict_entities(text, labels, threshold=0.3)
            result_json = json.dumps(entities, ensure_ascii=False)
            
            choice.delta.role = "assistant"
            choice.delta.content = result_json
            choice.finish_reason = "stop"

            if request.stream:
                yield response
            else:
                return response
        except Exception as e:
            print(f"Error evaluating ChatCompletion: {e}")
            if request.stream:
                yield response
            else:
                return response

    def Embeddings(self, request, context):
        response = inference_pb2.EmbeddingResponse()
        response.object = "list"
        response.model = request.model_id
        
        try:
            texts = list(request.input)
            inputs = self.tokenizer(texts, padding=True, truncation=True, return_tensors="pt")
            with torch.no_grad():
                outputs = self.base_encoder(**inputs)
                # Mean pool the embeddings to generate standard vector response
                embeddings = outputs.last_hidden_state.mean(dim=1).cpu().numpy().tolist()

            for i, emb in enumerate(embeddings):
                data = response.data.add()
                data.object = "embedding"
                data.index = i
                data.embedding.extend(emb)

        except Exception as e:
            print(f"Error evaluating Embeddings: {e}")
        
        return response

def serve():
    # Provide generous thread pool for concurrent multi-queue validations
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    inference_pb2_grpc.add_InferenceServiceServicer_to_server(InferenceServiceServicer(), server)
    port = '[::]:50053'
    server.add_insecure_port(port)
    server.start()
    print(f"Type C Python ONNX Node listening on {port}")
    server.wait_for_termination()

if __name__ == '__main__':
    serve()
