#ifndef SERVER_BASE_H
#define SERVER_BASE_H

#include "inference.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tactorder {
namespace inference {

using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

class ServerBase : public InferenceService::Service {
public:
  ServerBase();
  virtual ~ServerBase();

  // -- InferenceService Implementation --
  Status ListModels(ServerContext *context, const ListModelsRequest *request,
                    ListModelsResponse *response) override;

  Status ChatCompletion(ServerContext *context, const ChatRequest *request,
                        ServerWriter<ChatResponse> *writer) override;

  Status Embeddings(ServerContext *context, const EmbeddingRequest *request,
                    EmbeddingResponse *response) override;

protected:
  // Core virtual method for subclasses to implement actual generation
  virtual void GenerateStream(
      const std::string &model_id,
      const std::vector<std::pair<std::string, std::string>> &messages,
      std::ostream *os) = 0;

  // Optional: Subclasses can override to load model metadata differently
  virtual void ScanModels();

  // Helpers
  std::string GetEnv(const std::string &key,
                     const std::string &default_val = "");

  // Model registry
  std::map<std::string, std::string> available_models_; // ID -> Path
  std::string models_dir_;
};

} // namespace inference
} // namespace tactorder

#endif // SERVER_BASE_H
