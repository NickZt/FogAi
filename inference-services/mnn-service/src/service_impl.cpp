#include "service_impl.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace fs = std::filesystem;

namespace tactorder {
namespace inference {

// --- GrpcStreamBuf Implementation ---

int GrpcStreamBuf::overflow(int c) {
  if (c != EOF) {
    buffer_ += static_cast<char>(c);
  }
  return c;
}

int GrpcStreamBuf::sync() {
  flush_buffer();
  return 0;
}

void GrpcStreamBuf::flush_buffer() {
  if (buffer_.empty())
    return;

  // Log progress every ~10 tokens/chunks to show activity without flooding
  token_count_++;
  if (token_count_ % 5 == 0) {
    std::cout << "." << std::flush;
  }

  ChatResponse response;
  response.set_id("chatcmpl-" + model_id_);
  response.set_created(time(nullptr));
  response.set_model(model_id_);

  auto *choice = response.add_choices();
  choice->set_index(0);
  auto *delta = choice->mutable_delta();
  delta->set_content(buffer_);

  writer_->Write(response);
  buffer_.clear();
}

// --- InferenceServiceImpl Implementation ---

InferenceServiceImpl::InferenceServiceImpl(const std::string &input_path,
                                           bool is_directory)
    : input_path_(input_path), is_directory_(is_directory) {
  ScanModels();

  // Auto-load if only one model is available (or passed as single file)
  if (available_models_.size() == 1) {
    GetOrLoadModel(available_models_.begin()->first);
  }
}

InferenceServiceImpl::~InferenceServiceImpl() = default;

void InferenceServiceImpl::ScanModels() {
  std::cout << "Scanning for models in: " << input_path_ << std::endl;
  available_models_.clear();

  if (!is_directory_) {
    // Single file mode (input_path is config.json)
    // Derive ID from parent directory name
    fs::path config_path(input_path_);
    std::string id = config_path.parent_path().filename().string();
    available_models_[id] = input_path_;
    std::cout << "Found model: " << id << " path: " << input_path_ << std::endl;
    return;
  }

  // Directory mode
  try {
    for (const auto &entry : fs::directory_iterator(input_path_)) {
      if (entry.is_directory()) {
        fs::path config_path = entry.path() / "config.json";
        if (fs::exists(config_path)) {
          std::string id = entry.path().filename().string();
          available_models_[id] = config_path.string();
          std::cout << "Found model: " << id
                    << " path: " << config_path.string() << std::endl;
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error scanning directory: " << e.what() << std::endl;
  }
}

MNN::Transformer::Llm *
InferenceServiceImpl::GetOrLoadModel(const std::string &model_id) {
  std::lock_guard<std::mutex> lock(model_mutex_);

  // Check if loaded
  auto it = loaded_models_.find(model_id);
  if (it != loaded_models_.end()) {
    return it->second.get();
  }

  // Check if available
  auto path_it = available_models_.find(model_id);
  if (path_it == available_models_.end()) {
    std::cerr << "Model ID not found: " << model_id << std::endl;
    return nullptr;
  }

  // Single Active Model Policy: Unload others
  if (!loaded_models_.empty()) {
    std::cout << "[System] Unloading previous models to free memory..."
              << std::endl;
    loaded_models_.clear();
  }

  // Load
  std::string config_path = path_it->second;
  std::cout << "[System] Loading model: " << model_id << " from " << config_path
            << " ..." << std::endl;
  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    std::unique_ptr<MNN::Transformer::Llm> new_llm(
        MNN::Transformer::Llm::createLLM(config_path.c_str()));

    if (!new_llm) {
      std::cerr << "[Error] Failed to create LLM for " << model_id << std::endl;
      return nullptr;
    }

    new_llm->load();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    std::cout << "[System] Model loaded successfully in " << duration.count()
              << " ms." << std::endl;

    loaded_models_[model_id] = std::move(new_llm);
    return loaded_models_[model_id].get();

  } catch (const std::exception &e) {
    std::cerr << "[Error] Exception loading model " << model_id << ": "
              << e.what() << std::endl;
    return nullptr;
  }
}

Status InferenceServiceImpl::ListModels(ServerContext *context,
                                        const ListModelsRequest *request,
                                        ListModelsResponse *response) {
  for (const auto &kv : available_models_) {
    auto *card = response->add_models();
    card->set_id(kv.first);
    card->set_object("model");
    card->set_created(1700000000); // Placeholder
    card->set_owned_by("tactorder");
  }
  return Status::OK;
}

Status
InferenceServiceImpl::ChatCompletion(ServerContext *context,
                                     const ChatRequest *request,
                                     ServerWriter<ChatResponse> *writer) {
  std::string model_id = request->model_id();
  std::cout << "Received request for model: " << model_id << std::endl;

  MNN::Transformer::Llm *llm = GetOrLoadModel(model_id);

  if (!llm) {
    // Fallback: Try loading first available model if user requested something
    // else but we only have one? No, let's keep it strict. Or maybe default if
    // empty ID?
    if (model_id.empty() && !available_models_.empty()) {
      llm = GetOrLoadModel(available_models_.begin()->first);
    }
  }

  if (!llm) {
    std::cerr << "Model not found or failed to load: " << model_id << std::endl;
    return Status(grpc::INTERNAL, "Model not loaded: " + model_id);
  }

  std::vector<std::pair<std::string, std::string>> prompt_items;
  for (const auto &msg : request->messages()) {
    std::string role = msg.role();
    std::string content = msg.content();

    // Attempt to parse if looks like JSON array
    if (!content.empty() && content.front() == '[') {
      try {
        auto j = json::parse(content);
        if (j.is_array()) {
          std::string combined_text;
          for (const auto &item : j) {
            if (item.is_object() && item.contains("type")) {
              std::string type = item["type"];
              if (type == "text" && item.contains("text")) {
                combined_text += item["text"].get<std::string>();
              } else if (type == "image_url" && item.contains("image_url")) {
                std::string url = item["image_url"]["url"];
                // TODO: Load image from URL and pass to LLM.
                // For now, format as text description or Qwen2-VL prompt
                // placeholder combined_text +=
                // "<|vision_start|><|image_pad|><|vision_end|>";
                combined_text += "\n[Image: " + url + "]\n";
                std::cout << "Detected Image URL: " << url << std::endl;
              }
            }
          }
          content = combined_text;
        }
      } catch (json::parse_error &e) {
        // Ignore, treat as raw text
      }
    }

    if (role == "user") {
      prompt_items.emplace_back(content, "");
    } else if (role == "assistant") {
      if (!prompt_items.empty()) {
        prompt_items.back().second = content;
      }
    } else if (role == "system") {
      // Prepend system prompt to next user message or just ignore for now if
      // not supported via API Simple hack: append to next user message if
      // vector empty, or previous? For now, let's ignore or print
      std::cout << "System prompt ignored (TODO: Implement): " << content
                << std::endl;
    }
  }

  GrpcStreamBuf stream_buf(writer, model_id);
  std::ostream os(&stream_buf);

  std::cout << "[System] Starting generation..." << std::flush;
  try {
    llm->response(prompt_items, &os);
  } catch (const std::exception &e) {
    std::cerr << "\n[Error] Error during inference: " << e.what() << std::endl;
    return Status(grpc::INTERNAL, "Inference failed: " + std::string(e.what()));
  }

  os.flush();
  os.flush();
  std::cout << "\n[System] Generation finished." << std::endl;

  // Log Metrics
  auto llm_context = llm->getContext();
  if (llm_context) {
    double prefill_ms = llm_context->prefill_us / 1000.0;
    double decode_ms = llm_context->decode_us / 1000.0;
    double speed = 0.0;
    if (llm_context->decode_us > 0) {
      speed = llm_context->gen_seq_len / (llm_context->decode_us / 1000000.0);
    }
    std::cout << "[Metrics] Prompt Tokens: " << llm_context->prompt_len << "\n"
              << "[Metrics] Generated Tokens: " << llm_context->gen_seq_len
              << "\n"
              << "[Metrics] Prefill Time: " << prefill_ms << " ms\n"
              << "[Metrics] Decode Time: " << decode_ms << " ms\n"
              << "[Metrics] Speed: " << speed << " tokens/sec" << std::endl;
  }

  ChatResponse final_response;
  final_response.set_id("chatcmpl-" + model_id);
  final_response.set_created(time(nullptr));
  final_response.set_model(model_id);
  auto *choice = final_response.add_choices();
  choice->set_index(0);
  choice->set_finish_reason("stop");
  choice->mutable_delta();
  writer->Write(final_response);

  return Status::OK;
}

Status InferenceServiceImpl::Embeddings(ServerContext *context,
                                        const EmbeddingRequest *request,
                                        EmbeddingResponse *response) {
  return Status::OK;
}

} // namespace inference
} // namespace tactorder
