import sys
import os
import json
import time
from concurrent import futures
import grpc
import torch
from dotenv import load_dotenv

load_dotenv()

# Append MNNLLama root to sys.path to access compiled Protobufs
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
if PROJECT_ROOT not in sys.path:
    sys.path.append(PROJECT_ROOT)

import inference_pb2
import inference_pb2_grpc
from gliner import GLiNER
from transformers import AutoModel, AutoTokenizer, AutoModelForCausalLM

class InferenceServiceServicer(inference_pb2_grpc.InferenceServiceServicer):
    def __init__(self):
        self.models_dir = os.environ.get("ONNX_MODELS_DIR", "../../models_onnx")
        self.catalog = {}
        self.loaded_models = {}
        self._scan_models()

    def _scan_models(self):
        print(f"Scanning for models in {self.models_dir}...")
        if not os.path.isdir(self.models_dir):
            print("Models directory not found!")
            return
            
        for entry in os.scandir(self.models_dir):
            if entry.is_dir():
                self.catalog[entry.name] = entry.path
                print(f"Found model: {entry.name} at {entry.path}")

    def _load_model(self, model_name):
        if model_name in self.loaded_models:
            return self.loaded_models[model_name]
            
        path = self.catalog.get(model_name)
        if not path:
            raise ValueError(f"Model {model_name} not found in catalog")
            
        print(f"Loading model {model_name} into memory...")
        if "gliner" in model_name.lower():
            gliner_model = GLiNER.from_pretrained(path)
            gliner_model.eval()
            base_encoder = AutoModel.from_pretrained(path)
            base_encoder.eval()
            tokenizer = AutoTokenizer.from_pretrained(path)
            
            self.loaded_models[model_name] = {
                "type": "gliner",
                "gliner": gliner_model,
                "base": base_encoder,
                "tokenizer": tokenizer
            }
        else:
            tokenizer = AutoTokenizer.from_pretrained(path)
            # Default to float16 to save memory on heavy LLMs
            model = AutoModelForCausalLM.from_pretrained(path, torch_dtype=torch.float16, device_map="auto")
            model.eval()
            self.loaded_models[model_name] = {
                "type": "generic",
                "model": model,
                "tokenizer": tokenizer
            }
        print(f"Successfully loaded {model_name}.")
        return self.loaded_models[model_name]

    def ListModels(self, request, context):
        res = inference_pb2.ListModelsResponse()
        for model_name in self.catalog.keys():
            card = res.models.add()
            # Mark specialized feature pipelines with the internal suffix
            if "gliner" in model_name.lower():
                card.id = f"{model_name}-internal"
            else:
                card.id = model_name
                
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
        
        target_model = request.model_id
        if target_model.endswith("-internal"):
            target_model = target_model[:-9]

        try:
            model_cache = self._load_model(target_model)
            prompt = request.messages[0].content
            
            if model_cache["type"] == "gliner":
                try:
                    data = json.loads(prompt)
                    text = data.get("text", "")
                    labels = data.get("labels", [])
                except json.JSONDecodeError:
                    text = prompt
                    labels = ["person", "location", "date"]

                entities = model_cache["gliner"].predict_entities(text, labels, threshold=0.3)
                result_text = json.dumps(entities, ensure_ascii=False)
            else:
                tokenizer = model_cache["tokenizer"]
                model = model_cache["model"]
                inputs = tokenizer(prompt, return_tensors="pt").to(model.device)
                with torch.no_grad():
                    outputs = model.generate(**inputs, max_new_tokens=100)
                result_text = tokenizer.decode(outputs[0][inputs.input_ids.shape[1]:], skip_special_tokens=True)
            
            if request.stream:
                choice.delta.role = "assistant"
                choice.delta.content = result_text
            else:
                choice.message.role = "assistant"
                choice.message.content = result_text
                
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
        
        target_model = request.model_id
        if target_model.endswith("-internal"):
            target_model = target_model[:-9]
            
        try:
            model_cache = self._load_model(target_model)
            texts = list(request.input)
            tokenizer = model_cache["tokenizer"]
            
            if model_cache["type"] == "gliner":
                encoder = model_cache["base"]
            else:
                encoder = getattr(model_cache["model"], "model", model_cache["model"])

            inputs = tokenizer(texts, padding=True, truncation=True, return_tensors="pt").to(encoder.device)
            with torch.no_grad():
                outputs = encoder(**inputs)
                if hasattr(outputs, 'last_hidden_state'):
                    embeddings = outputs.last_hidden_state.mean(dim=1).cpu().numpy().tolist()
                else:
                    embeddings = outputs[0].mean(dim=1).cpu().numpy().tolist()

            for i, emb in enumerate(embeddings):
                data = response.data.add()
                data.object = "embedding"
                data.index = i
                data.embedding.extend(emb)

        except Exception as e:
            print(f"Error evaluating Embeddings: {e}")
        
        return response

def serve():
    port = os.environ.get("PORT", "50053")
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    inference_pb2_grpc.add_InferenceServiceServicer_to_server(InferenceServiceServicer(), server)
    addr = f'[::]:{port}'
    server.add_insecure_port(addr)
    server.start()
    print(f"Type C Python ONNX Node listening on {addr}")
    server.wait_for_termination()

if __name__ == '__main__':
    serve()
