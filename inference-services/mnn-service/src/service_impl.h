#ifndef SERVICE_IMPL_H
#define SERVICE_IMPL_H

#include "tactorder/inference/server_base.h"
#include <llm/llm.hpp> // High-level LLM API
#include <map>
#include <memory>
#include <mutex>

namespace tactorder {
namespace inference {

class InferenceServiceImpl final : public ServerBase {
public:
  InferenceServiceImpl(const std::string &input_path, bool is_directory);
  ~InferenceServiceImpl() override;

  // We rely on ServerBase for ChatCompletion scaffolding.
  // We override Embeddings to matching existing empty implementation if needed,
  // or just use ServerBase's UNIMPLEMENTED. Let's keep it compatible.
  Status Embeddings(ServerContext *context, const EmbeddingRequest *request,
                    EmbeddingResponse *response) override;

protected:
  void GenerateStream(
      const std::string &model_id,
      const std::vector<std::pair<std::string, std::string>> &messages,
      std::ostream *os) override;

  void ScanModels() override;

private:
  // Returns pointer to loaded LLM, or nullptr if failed.
  MNN::Transformer::Llm *GetOrLoadModel(const std::string &model_id);

  bool is_directory_; // We might still need this flag or infer it

  // model_id -> llm instance
  std::map<std::string, std::unique_ptr<MNN::Transformer::Llm>> loaded_models_;
  std::mutex model_mutex_; // Thread safety for loading
};

} // namespace inference
} // namespace tactorder

#endif // SERVICE_IMPL_H
