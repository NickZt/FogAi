#ifndef ONNX_SERVICE_IMPL_H
#define ONNX_SERVICE_IMPL_H

#include "tactorder/inference/server_base.h"
#include <map>
#include <memory>
#include <onnxruntime_cxx_api.h>

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

  Ort::Env env_;
  std::map<std::string, std::unique_ptr<Ort::Session>> sessions_;
};

} // namespace inference
} // namespace tactorder

#endif // ONNX_SERVICE_IMPL_H
