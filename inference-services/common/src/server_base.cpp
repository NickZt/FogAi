#include "tactorder/inference/server_base.h"
#include "tactorder/inference/grpc_stream_buf.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace tactorder {
namespace inference {

ServerBase::ServerBase() {
  // Default implementation of Env loading is simple here, or we can use
  // dotenv-cpp For now, let's just rely on getenv or custom ScanModels which
  // checks ENV.
}

ServerBase::~ServerBase() = default;

std::string ServerBase::GetEnv(const std::string &key,
                               const std::string &default_val) {
  const char *val = std::getenv(key.c_str());
  if (val == nullptr) {
    return default_val;
  }
  return std::string(val);
}

void ServerBase::ScanModels() {
  if (models_dir_.empty())
    return;

  std::cout << "Scanning for models in: " << models_dir_ << std::endl;
  available_models_.clear();

  if (!fs::exists(models_dir_)) {
    std::cerr << "Models directory does not exist: " << models_dir_
              << std::endl;
    return;
  }

  try {
    for (const auto &entry : fs::directory_iterator(models_dir_)) {
      if (entry.is_directory()) {
        // Heuristic: If it has config.json (MNN) or config.json/tokenizer.json
        // (ONNX/HF) For now, simpler: Just list directories as models.
        std::string id = entry.path().filename().string();
        available_models_[id] = entry.path().string();
        std::cout << "Found model: " << id << " path: " << entry.path().string()
                  << std::endl;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error scanning directory: " << e.what() << std::endl;
  }
}

Status ServerBase::ListModels(ServerContext *context,
                              const ListModelsRequest *request,
                              ListModelsResponse *response) {
  if (available_models_.empty()) {
    ScanModels(); // Lazy scan if empty?
  }

  for (const auto &kv : available_models_) {
    auto *card = response->add_models();
    card->set_id(kv.first);
    card->set_object("model");
    card->set_created(1700000000);
    card->set_owned_by("tactorder");
  }
  return Status::OK;
}

Status ServerBase::ChatCompletion(ServerContext *context,
                                  const ChatRequest *request,
                                  ServerWriter<ChatResponse> *writer) {
  std::string model_id = request->model_id();
  std::cerr << "Entering ChatCompletion for model: " << model_id << std::endl;
  std::cout << "Received request for model: " << model_id << std::endl;

  // Validate model existence
  if (available_models_.find(model_id) == available_models_.end()) {
    // Try rescanning once
    ScanModels();
    if (available_models_.find(model_id) == available_models_.end()) {
      return Status(grpc::NOT_FOUND, "Model not found: " + model_id);
    }
  }

  // Prepare messages
  std::vector<std::pair<std::string, std::string>> prompt_items;
  for (const auto &msg : request->messages()) {
    prompt_items.emplace_back(
        msg.content(),
        ""); // Simplification: assuming user/system roles map to content.
    // Logic refinement needed for roles, but sticking to existing MNN pattern
    // for now: MNN implementation was: pair<string, string> where? Ah, checked
    // service_impl.cpp: user -> emplace_back(content, "") assistant ->
    // back().second = content system -> ignore/print

    // We should replicate that logic or Pass raw messages to GenerateStream?
    // Better to pass raw messages and let subclass handle formatting?
    // But GenerateStream signature in header used vector<pair>.
    // Let's replicate MNN logic here for shared use.

    std::string role = msg.role();
    std::string content = msg.content();
    prompt_items.emplace_back(role, content);
  }

  GrpcStreamBuf stream_buf(writer, model_id);
  std::ostream os(&stream_buf);

  try {
    GenerateStream(model_id, prompt_items, &os);
  } catch (const std::exception &e) {
    std::cerr << "\n[Error] Error during inference: " << e.what() << std::endl;
    return Status(grpc::INTERNAL, "Inference failed: " + std::string(e.what()));
  }

  os.flush();

  // Final stop chunk
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

Status ServerBase::Embeddings(ServerContext *context,
                              const EmbeddingRequest *request,
                              EmbeddingResponse *response) {
  return Status(grpc::UNIMPLEMENTED,
                "Embeddings not yet implemented in common base");
}

} // namespace inference
} // namespace tactorder
