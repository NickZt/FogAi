#ifndef ONNX_SERVICE_IMPL_H
#define ONNX_SERVICE_IMPL_H

#include "tactorder/inference/server_base.h"
#include "tactorder/inference/tokenizer.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <onnxruntime_cxx_api.h>
#include <vector>

namespace tactorder {
namespace inference {

class OnnxServiceImpl final : public ServerBase {
public:
  OnnxServiceImpl(const std::string &models_dir);
  ~OnnxServiceImpl() override;

protected:
  // Core generation logic using ORT
  void GenerateStream(
      const std::string &model_id,
      const std::vector<std::pair<std::string, std::string>> &messages,
      std::ostream *os) override;

private:
  // Helper to load session
  Ort::Session *GetOrLoadSession(const std::string &model_id);
  // Helper to load tokenizer
  Tokenizer *GetOrLoadTokenizer(const std::string &model_id);

  Ort::Env env_;
  std::map<std::string, std::unique_ptr<Ort::Session>> sessions_;
  std::map<std::string, std::unique_ptr<Tokenizer>> tokenizers_;
};

} // namespace inference
} // namespace tactorder

#endif // ONNX_SERVICE_IMPL_H
