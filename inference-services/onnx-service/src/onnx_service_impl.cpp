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

Tokenizer *OnnxServiceImpl::GetOrLoadTokenizer(const std::string &model_id) {
  auto it = tokenizers_.find(model_id);
  if (it != tokenizers_.end()) {
    return it->second.get();
  }

  // We assume GetOrLoadSession has been called or we can find the path
  // similarly
  auto path_it = available_models_.find(model_id);
  if (path_it == available_models_.end())
    return nullptr;

  // Find tokenizer.txt
  // Assuming it's in the same dir as the .onnx file (or parent of recursive
  // search?) Simpler: Just look in the model_path directory.
  std::string model_dir = path_it->second;
  // If model_path points to a file (from recursive search in GetOrLoadSession
  // cache?), Wait, available_models_ stores the *root* directory for that model
  // usually, unless ScanModels updated it? ServerBase::ScanModels stores the
  // directory path.

  std::string input_path = model_dir + "/tokenizer.txt";
  // Check if it exists?
  // Also check subdirectory if we did recursive search for onnx
  // For now, let's assume it's at top level of model dir
  if (!std::filesystem::exists(input_path)) {
    // Fallback: look in subdirectories?
    // For Qwen, we exported to root of ONNX_MODELS_DIR/Qwen...
    // So it should be there.
  }

  std::cout << "Loading tokenizer from: " << input_path << std::endl;
  auto tokenizer = Tokenizer::createTokenizer(input_path);
  if (!tokenizer) {
    std::cerr << "Failed to load tokenizer." << std::endl;
    return nullptr;
  }
  tokenizers_[model_id] = std::move(tokenizer);
  return tokenizers_[model_id].get();
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

  Tokenizer *tokenizer = GetOrLoadTokenizer(model_id);
  if (!tokenizer) {
    *os << "Error: Tokenizer not loaded.\n";
    return;
  }

  // 1. Chat Template (Simplified ChatML for Qwen)
  std::stringstream prompt_ss;
  // Initialize with system prompt if needed, but ServerBase currently doesn't
  // pass it in vector<pair>. Assuming default system prompt or relying on user
  // to include it?
  prompt_ss << "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n";

  for (const auto &msg : messages) {
    std::string user_content = msg.first;
    std::string assistant_content = msg.second;

    if (!user_content.empty()) {
      prompt_ss << "<|im_start|>user\n" << user_content << "<|im_end|>\n";
    }
    if (!assistant_content.empty()) {
      prompt_ss << "<|im_start|>assistant\n"
                << assistant_content << "<|im_end|>\n";
    }
  }
  // Add generation prompt
  prompt_ss << "<|im_start|>assistant\n";
  std::string prompt = prompt_ss.str();

  // 2. Tokenize
  std::vector<int> input_ids = tokenizer->encode(prompt);

  for (int id : input_ids) {
    *os << tokenizer->decode(id) << std::flush;
  }
  *os << "\n[Response Generation Placeholder]\n";

  // 3. Inference (Simulated for now, just proving tokenizer works)
  // Input tensor preparation requires correct shapes (batch_size, seq_len)
  // And 64-bit int tensors.

  /*
  std::vector<int64_t> input_ids_64;
  input_ids_64.reserve(input_ids.size());
  for(int id : input_ids) input_ids_64.push_back(id);

  std::array<int64_t, 2> input_shape = {1, (int64_t)input_ids_64.size()};

  Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,
  OrtMemTypeDefault); Ort::Value input_tensor =
  Ort::Value::CreateTensor<int64_t>(memory_info, input_ids_64.data(),
  input_ids_64.size(), input_shape.data(), input_shape.size());

  // Running session would require knowing output names and handling potential
  errors
  // session->Run(Ort::RunOptions{nullptr}, ...);
  */

  *os << "Tokenizer verified!\n";
}

} // namespace inference
} // namespace tactorder
