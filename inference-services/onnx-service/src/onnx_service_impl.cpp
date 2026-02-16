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
  std::vector<int32_t> input_ids_int32 = tokenizer->encode(prompt);
  std::vector<int64_t> current_input_ids;
  current_input_ids.reserve(input_ids_int32.size());
  for (int32_t id : input_ids_int32)
    current_input_ids.push_back(static_cast<int64_t>(id));

  std::string pending_bytes; // Buffer for partial UTF-8 sequences

  // 3. Initialize Loop State
  int64_t seq_len = current_input_ids.size();
  int64_t past_seq_len = 0;
  int m_max_tokens = 512; // Hardcoded max tokens for now
  int token_count = 0;

  // KV Cache: 24 layers, 2 tensors (key, value) per layer
  // Shape: [Batch=1, NumHeads=16, PastSeqLen, HeadDim=64]
  // Initialize with empty tensors (PastSeqLen = 0)
  std::vector<Ort::Value> past_kv_tensors;
  int num_layers = 24;
  Ort::AllocatorWithDefaultOptions allocator;

  for (int i = 0; i < num_layers * 2; ++i) {
    int64_t kv_shape[] = {1, 16, 0, 64};
    // Creating zero-length tensor for initial run
    past_kv_tensors.push_back(
        Ort::Value::CreateTensor<float>(allocator, kv_shape, 4));
  }

  // 4. Inference Loop
  try {
    while (token_count < m_max_tokens) {
      Ort::MemoryInfo memory_info =
          Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

      // A. Prepare Inputs
      std::vector<const char *> input_names;
      std::vector<Ort::Value> input_tensors;

      // input_ids
      input_names.push_back("input_ids");
      int64_t input_ids_shape[] = {1, (int64_t)current_input_ids.size()};
      input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
          memory_info, current_input_ids.data(), current_input_ids.size(),
          input_ids_shape, 2));

      // attention_mask
      input_names.push_back("attention_mask");
      std::vector<int64_t> attention_mask(
          past_seq_len + current_input_ids.size(), 1);
      int64_t mask_shape[] = {1, (int64_t)attention_mask.size()};
      input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
          memory_info, attention_mask.data(), attention_mask.size(), mask_shape,
          2));

      // position_ids
      input_names.push_back("position_ids");
      std::vector<int64_t> position_ids;
      for (int64_t i = 0; i < current_input_ids.size(); ++i) {
        position_ids.push_back(past_seq_len + i);
      }
      int64_t pos_shape[] = {1, (int64_t)position_ids.size()};
      input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
          memory_info, position_ids.data(), position_ids.size(), pos_shape, 2));

      // past_key_values
      // We need to keep names persistent if using c_str()
      static std::vector<std::string> past_kv_names;
      if (past_kv_names.empty()) {
        for (int i = 0; i < num_layers; ++i) {
          past_kv_names.push_back("past_key_values." + std::to_string(i) +
                                  ".key");
          past_kv_names.push_back("past_key_values." + std::to_string(i) +
                                  ".value");
        }
      }
      for (size_t i = 0; i < past_kv_names.size(); ++i) {
        input_names.push_back(past_kv_names[i].c_str());
        // We must push copies of Ort::Value as they do not support copy
        // assignment cleanly without move. But wait, we can just push move
        // objects? No, past_kv_tensors holds the state. Ort::Value is
        // move-only? We need to pass references or move them in and then move
        // them back? ORT Run expects vector of values. Actually, we can just
        // *move* them into input_tensors, and then *replace* them with outputs.
        input_tensors.push_back(std::move(past_kv_tensors[i]));
      }

      // Prepare Output Names
      std::vector<const char *> output_names;
      output_names.push_back("logits");
      static std::vector<std::string> present_kv_names;
      if (present_kv_names.empty()) {
        for (int i = 0; i < num_layers; ++i) {
          present_kv_names.push_back("present." + std::to_string(i) + ".key");
          present_kv_names.push_back("present." + std::to_string(i) + ".value");
        }
      }
      for (const auto &name : present_kv_names)
        output_names.push_back(name.c_str());

      // B. Run Session
      auto output_tensors = session->Run(
          Ort::RunOptions{nullptr}, input_names.data(), input_tensors.data(),
          input_tensors.size(), output_names.data(), output_names.size());

      // C. Process Outputs

      // 1. Logits -> Next Token
      float *logits_raw = output_tensors[0].GetTensorMutableData<float>();
      auto logits_type_info = output_tensors[0].GetTensorTypeAndShapeInfo();
      auto logits_shape = logits_type_info.GetShape();
      // Shape: [Batch, SeqLen, VocabSize]
      int64_t batch_size = logits_shape[0];
      int64_t seq_len_out = logits_shape[1];
      int64_t vocab_size = logits_shape[2];

      // Get logits for the last token in the sequence
      float *last_token_logits =
          logits_raw + (batch_size * seq_len_out - 1) * vocab_size;

      // Greedy Sampling (Argmax)
      int next_token_id = 0;
      float max_logit = -1e9;
      for (int v = 0; v < vocab_size; ++v) {
        if (last_token_logits[v] > max_logit) {
          max_logit = last_token_logits[v];
          next_token_id = v;
        }
      }

      // D. Update State for Next Iteration
      std::string decoded_token = tokenizer->decode(next_token_id);

      // UTF-8 Buffering Logic
      pending_bytes += decoded_token;

      // Helper lambda to find cutoff index for valid UTF-8
      auto get_valid_utf8_len = [](const std::string &s) -> size_t {
        if (s.empty())
          return 0;
        size_t i = s.size();
        size_t last_byte_idx = i - 1;
        unsigned char c = static_cast<unsigned char>(s[last_byte_idx]);

        // If last byte is ASCII, everything up to here is valid (assuming
        // prefix was valid)
        if ((c & 0x80) == 0)
          return i;

        // If last byte is a start byte, it's incomplete (needs at least 2
        // bytes)
        if ((c & 0xC0) == 0xC0)
          return last_byte_idx; // Split before this byte

        // If continuation byte, look back for start
        // Scan back up to 4 bytes (max UTF-8 length)
        for (int k = 0; k < 4 && k <= last_byte_idx; ++k) {
          unsigned char prev = static_cast<unsigned char>(s[last_byte_idx - k]);
          if ((prev & 0xC0) == 0xC0) {
            // Found start byte. Check if we have enough bytes.
            int needed = 0;
            if ((prev & 0xE0) == 0xC0)
              needed = 2;
            else if ((prev & 0xF0) == 0xE0)
              needed = 3;
            else if ((prev & 0xF8) == 0xF0)
              needed = 4;

            if (k + 1 >= needed)
              return i; // Complete
            else
              return last_byte_idx - k; // Incomplete, split before start byte
          }
        }
        // If we didn't find a start byte within lookback, it's either invalid
        // or we missed the start. Assume valid prefix + incomplete tail? Or
        // just flush? For safety, let's assume if we can't find start, we keep
        // it buffered if it's short, or flush if long.
        if (s.size() > 10)
          return i; // Flush if buffer getting too big to be just one char
        return 0;   // Keep buffering
      };

      size_t valid_len = get_valid_utf8_len(pending_bytes);
      if (valid_len > 0) {
        *os << pending_bytes.substr(0, valid_len) << std::flush;
        pending_bytes = pending_bytes.substr(valid_len);
      }

      past_seq_len += current_input_ids.size();
      current_input_ids.clear();
      current_input_ids.push_back(next_token_id);

      // KV Cache Update: Output 'present' becomes 'past_key_values' for next
      // run
      past_kv_tensors.clear();
      for (size_t i = 1; i < output_tensors.size(); ++i) {
        past_kv_tensors.push_back(std::move(output_tensors[i]));
      }

      token_count++;

      // Stop Conditions
      if (decoded_token == "<|im_end|>" || decoded_token == "<|endoftext|>")
        break;
      // Also check specific token IDs if known (e.g., 151645 for im_end)
    }
  } catch (const Ort::Exception &e) {
    *os << "\n[Error] ONNX Runtime Error: " << e.what() << "\n";
    std::cerr << "ORT Error: " << e.what() << std::endl;
  }
  *os << "\n"; // End of stream
}

} // namespace inference
} // namespace tactorder
