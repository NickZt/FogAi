#include "service_impl.h"
#include <MNN/Interpreter.hpp>
#include <MNN/expr/Executor.hpp>
#include <MNN/expr/ExprCreator.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace tactorder {
namespace inference {

InferenceServiceImpl::InferenceServiceImpl(const std::string &model_path)
    : model_path_(model_path) {
  if (LoadModel()) {
    std::cout << "MNN Model loaded successfully from: " << model_path_
              << std::endl;
  } else {
    std::cerr << "Failed to load MNN Model from: " << model_path_ << std::endl;
  }
}

InferenceServiceImpl::~InferenceServiceImpl() = default;

bool InferenceServiceImpl::LoadModel() {
  try {
    MNN::ScheduleConfig config;
    MNN::BackendConfig backendConfig;
    config.type = MNN_FORWARD_CPU;
    config.numThread = 4; // Adjust as needed
    backendConfig.precision = MNN::BackendConfig::Precision_Low;
    backendConfig.memory = MNN::BackendConfig::Memory_Low;
    config.backendConfig = &backendConfig;

    std::shared_ptr<MNN::Express::Executor::RuntimeManager> runtime_manager(
        MNN::Express::Executor::RuntimeManager::createRuntimeManager(config));

    // Set external file for weights (model_path + ".weight")
    std::string external_weight_path = model_path_ + ".weight";
    runtime_manager->setExternalFile(external_weight_path);

    MNN::Express::Module::Config module_config;
    module_config.shapeMutable = true;
    module_config.rearrange = true;

    std::cout << "Loading MNN Module with external weights from: "
              << external_weight_path << std::endl;

    // Attempt load with standard LLM inputs/outputs
    // Qwen2-0.5B-Instruct-MNN uses "logits" and "presents" (or
    // "past_key_values") We try "logits" as the primary output.
    llm_module_.reset(MNN::Express::Module::load(
        {"input_ids", "attention_mask", "position_ids", "past_key_values"},
        {"logits", "presents"}, model_path_.c_str(), runtime_manager,
        &module_config));

    if (llm_module_) {
      std::cout << "Loaded via MNN::Express::Module with RuntimeManager!"
                << std::endl;
      return true;
    }

    std::cerr << "Module load returned null." << std::endl;
    return false;
  } catch (const std::exception &e) {
    std::cerr << "Exception loading model: " << e.what() << std::endl;
    return false;
  }
}

Status
InferenceServiceImpl::ChatCompletion(ServerContext *context,
                                     const ChatRequest *request,
                                     ServerWriter<ChatResponse> *writer) {
  std::cout << "Received Chat Completion Request for model: "
            << request->model_id() << std::endl;

  std::string response_text = "Model " + request->model_id() + " is loaded. ";
  if (llm_module_) {
    response_text += "MNN Module is ready (Logits available). ";
  } else {
    response_text += "MNN Module failed to load.";
  }

  ChatResponse response;
  response.set_id(
      "chatcmpl-mnn-" +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count()));
  response.set_created(time(nullptr));
  response.set_model(request->model_id());

  auto *choice = response.add_choices();
  choice->set_index(0);
  auto *delta = choice->mutable_delta();
  delta->set_role("assistant");
  delta->set_content(response_text);

  writer->Write(response);

  // Finish
  ChatResponse final_response = response;
  final_response.mutable_choices(0)->set_finish_reason("stop");
  final_response.mutable_choices(0)->mutable_delta()->set_content("");
  writer->Write(final_response);

  return Status::OK;
}

Status InferenceServiceImpl::Embeddings(ServerContext *context,
                                        const EmbeddingRequest *request,
                                        EmbeddingResponse *response) {
  std::cout << "Received Embeddings Request for model: " << request->model_id()
            << std::endl;
  // Mock response
  response->set_object("list");
  response->set_model(request->model_id());

  auto *embedding = response->add_data();
  embedding->set_object("embedding");
  embedding->set_index(0);
  embedding->add_embedding(0.1f);
  embedding->add_embedding(0.2f);
  embedding->add_embedding(0.3f);

  return Status::OK;
}

} // namespace inference
} // namespace tactorder
