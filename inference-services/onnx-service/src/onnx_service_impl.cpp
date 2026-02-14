#include "onnx_service_impl.h"
#include <filesystem>
#include <iostream>
#include <numeric>
#include <vector>

namespace tactorder {
namespace inference {

OnnxServiceImpl::OnnxServiceImpl(const std::string &models_dir)
    : env_(ORT_LOGGING_LEVEL_WARNING, "OnnxService") {
  models_dir_ = models_dir;
  ScanModels();
}

OnnxServiceImpl::~OnnxServiceImpl() = default;

Ort::Session *OnnxServiceImpl::GetOrLoadSession(const std::string &model_id) {
  auto it = sessions_.find(model_id);
  if (it != sessions_.end()) {
    return it->second.get();
  }

  auto path_it = available_models_.find(model_id);
  if (path_it == available_models_.end()) {
    std::cerr << "Model ID not found: " << model_id << std::endl;
    return nullptr;
  }

  std::string model_path = path_it->second;

  // Helper to find .onnx file recursively
  if (std::filesystem::is_directory(model_path)) {
    std::string best_path;
    int max_priority = -1;

    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(model_path)) {
      if (entry.is_regular_file() && entry.path().extension() == ".onnx") {
        std::string filename = entry.path().filename().string();
        int priority = 0;

        if (filename == "model_q4_k_m.onnx")
          priority = 10;
        else if (filename == "model_q4.onnx")
          priority = 9;
        else if (filename == "model_int4.onnx")
          priority = 8;
        else if (filename == "model_quantized.onnx")
          priority = 7;
        else if (filename == "model_int8.onnx")
          priority = 7;
        else if (filename == "model.onnx")
          priority = 6;
        else if (filename == "decoder_model_merged_quantized.onnx")
          priority = 5;
        else if (filename == "decoder_model_merged.onnx")
          priority = 4;
        else
          priority = 1;

        if (priority > max_priority) {
          max_priority = priority;
          best_path = entry.path().string();
        }
      }
    }
    if (!best_path.empty()) {
      model_path = best_path;
    }
  }

  // Still directory? Then we failed to find .onnx
  if (std::filesystem::is_directory(model_path)) {
    std::cerr << "No .onnx file found in: " << model_path << std::endl;
    return nullptr;
  }

  std::cout << "Loading ONNX model from: " << model_path << std::endl;
  try {
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);

    auto session = std::make_unique<Ort::Session>(env_, model_path.c_str(),
                                                  session_options);
    sessions_[model_id] = std::move(session);
    std::cout << "Model loaded successfully." << std::endl;
    return sessions_[model_id].get();
  } catch (const Ort::Exception &e) {
    std::cerr << "Error loading model: " << e.what() << std::endl;
    return nullptr;
  }
}

void OnnxServiceImpl::GenerateStream(
    const std::string &model_id,
    const std::vector<std::pair<std::string, std::string>> &messages,
    std::ostream *os) {
  Ort::Session *session = GetOrLoadSession(model_id);
  if (!session) {
    *os << "Error: Model not loaded." << std::flush;
    return;
  }

  // TODO: Integrate a Tokenizer (e.g. tokenizers-cpp)
  // For now, we prove the pipeline works by running a dummy inference or just
  // echoing. If we want to run inference, we need input shape info.

  // Let's inspect model inputs to prove we have access
  Ort::AllocatorWithDefaultOptions allocator;
  size_t num_input_nodes = session->GetInputCount();
  std::cout << "Model Input Count: " << num_input_nodes << std::endl;
  /*
  for (size_t i = 0; i < num_input_nodes; i++) {
      char* input_name = session->GetInputName(i, allocator);
      std::cout << "Input " << i << ": " << input_name << std::endl;
      allocator.Free(input_name);
  }
  */

  // Placeholder response
  *os << "Hello from C++ ONNX Service! (Model Version: " << model_id << ")\n";
  *os << "Note: Tokenizer is not yet integrated, so actual inference is "
         "skipped.\n";
  *os << "But the model was loaded successfully into ONNX Runtime."
      << std::flush;
}

} // namespace inference
} // namespace tactorder
