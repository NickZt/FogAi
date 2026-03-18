import os
import sys

# Add tools to path to import export_tokenizer
sys.path.append(os.path.join(os.path.dirname(__file__), "tools"))
from export_tokenizer import LlmTokenizer

def main():
    model_id = "models_mnn/gliner-bi-v2"
    output_dir = "models_mnn/gliner-bi-v2"
    
    print(f"Loading tokenizer from {model_id}...")
    # Force use_fast=True
    from transformers import AutoTokenizer
    tokenizer = AutoTokenizer.from_pretrained(model_id, trust_remote_code=True, use_fast=True)
    
    # Wrap it
    class FastWrapper(LlmTokenizer):
        def __init__(self, tok, path):
            self.tokenizer = tok
            self.tokenizer_path = path
            self.model_type = "gemma3" # just to ensure token count isn't mismatched, wait, qwen2 is better
            self.stop_ids = []
            if hasattr(self.tokenizer, 'eos_token_id') and self.tokenizer.eos_token_id is not None:
                self.stop_ids.append(self.tokenizer.eos_token_id)
                
    wrapper = FastWrapper(tokenizer, model_id)
    wrapper.model_type = "qwen2"
    
    print(f"Exporting to {output_dir}...")
    wrapper.export(output_dir)
    print("Done!")

if __name__ == "__main__":
    main()
