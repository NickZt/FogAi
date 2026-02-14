#include "service_impl.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace tactorder {
namespace inference {

InferenceServiceImpl::InferenceServiceImpl(const std::string &input_path,
                                           bool is_directory)
    : is_directory_(is_directory) {
  models_dir_ = input_path; // Set base class member
  ScanModels();

  // Auto-load if only one model is available
  if (available_models_.size() == 1) {
    GetOrLoadModel(available_models_.begin()->first);
  }
}

InferenceServiceImpl::~InferenceServiceImpl() = default;

void InferenceServiceImpl::ScanModels() {
  std::cout << "Scanning for models in: " << models_dir_ << std::endl;
  available_models_.clear();

  if (!is_directory_) {
    // Single file mode
    fs::path config_path(models_dir_);
    std::string id = config_path.parent_path().filename().string();
    available_models_[id] = models_dir_;
    std::cout << "Found model: " << id << " path: " << models_dir_ << std::endl;
    return;
  }

  // Directory mode - MNN specific check for config.json
  try {
    for (const auto &entry : fs::directory_iterator(models_dir_)) {
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

  auto it = loaded_models_.find(model_id);
  if (it != loaded_models_.end()) {
    return it->second.get();
  }

  auto path_it = available_models_.find(model_id);
  if (path_it == available_models_.end()) {
    std::cerr << "Model ID not found: " << model_id << std::endl;
    return nullptr;
  }

  // Single Active Model Policy
  if (!loaded_models_.empty()) {
    std::cout << "[System] Unloading previous models to free memory..."
              << std::endl;
    loaded_models_.clear();
  }

  std::string config_path = path_it->second;

  // Simple memory check
  auto get_mem_available = []() -> float {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
      return -1.0f;
    char line[256];
    float mem_avail = -1.0f;
    while (fgets(line, sizeof(line), fp)) {
      if (sscanf(line, "MemAvailable: %f kB", &mem_avail) == 1) {
        mem_avail /= 1024.0f; // MB
        break;
      }
    }
    fclose(fp);
    return mem_avail;
  };

  float mem_before = get_mem_available();
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

    float mem_after = get_mem_available();
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

void InferenceServiceImpl::GenerateStream(
    const std::string &model_id,
    const std::vector<std::pair<std::string, std::string>> &messages,
    std::ostream *os) {
  MNN::Transformer::Llm *llm = GetOrLoadModel(model_id);
  if (!llm && available_models_.size() > 0) {
    // Fallback to first available if strictly needed, or just fail.
    // Base logic checks existence, but GetOrLoadModel handles loading.
    // If exact match failed, maybe we should try to load default?
    // Let's stick to exact match.
    if (model_id.empty()) {
      llm = GetOrLoadModel(available_models_.begin()->first);
    }
  }

  if (!llm) {
    throw std::runtime_error("Model not loaded: " + model_id);
  }

  // Process messages specifically for MNN (e.g. image parsing)
  // The messages passed here are already separated by role, but we might need
  // to do the JSON parsing for "content" if it's multimodal.
  // The previous implementation did this parsing on the request->messages()
  // loop. Here we have vector<pair<string, string>>. The first string is User
  // Content (or JSON), the second is Assistant Content.

  // We need to reconstruct this logic.
  // BUT! MNN::Transformer::Llm::response takes `vector<pair<string, string>>`.
  // It expects the content to be processed? OR does it handle vision tags?
  // The previous code MANUALLY parsed JSON to extract image URLs and append
  // tags. So we MUST process the messages here before passing to llm->response.

  std::vector<std::pair<std::string, std::string>> processed_messages;

  for (const auto &item : messages) {
    std::string user_content = item.first;
    std::string assistant_content = item.second;

    // JSON Vision Hack
    if (!user_content.empty() && user_content.front() == '[') {
      try {
        auto j = json::parse(user_content);
        if (j.is_array()) {
          std::string combined_text;
          for (const auto &item : j) {
            if (item.is_object() && item.contains("type")) {
              std::string type = item["type"];
              if (type == "text" && item.contains("text")) {
                combined_text += item["text"].get<std::string>();
              } else if (type == "image_url" && item.contains("image_url")) {
                std::string url = item["image_url"]["url"];
                combined_text += "\n[Image: " + url + "]\n";
                std::cout << "Detected Image URL: " << url << std::endl;
              }
            }
          }
          user_content = combined_text;
        }
      } catch (json::parse_error &) {
        // Ignore
      }
    }
    processed_messages.emplace_back(user_content, assistant_content);
  }

  std::cout << "[System] Starting generation..." << std::flush;
  llm->response(processed_messages, os);
  std::cout << "\n[System] Generation finished." << std::endl;

  // Log Metrics
  auto llm_context = llm->getContext();
  if (llm_context) {
    double prefill_ms = llm_context->prefill_us / 1000.0;
    double decode_ms = llm_context->decode_us / 1000.0;
    std::cout << "[Metrics] Prefill: " << prefill_ms
              << " ms, Decode: " << decode_ms << " ms" << std::endl;
  }
}

Status InferenceServiceImpl::Embeddings(ServerContext *context,
                                        const EmbeddingRequest *request,
                                        EmbeddingResponse *response) {
  return Status::OK;
}

} // namespace inference
} // namespace tactorder
