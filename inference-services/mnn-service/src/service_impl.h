#ifndef SERVICE_IMPL_H
#define SERVICE_IMPL_H

#include "inference.grpc.pb.h"
// #include <MNN/Interpreter.hpp>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <llm/llm.hpp> // High-level LLM API
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

namespace tactorder {
namespace inference {

using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

// Custom stream buffer to redirect std::ostream to gRPC ServerWriter
class GrpcStreamBuf : public std::streambuf {
public:
  GrpcStreamBuf(ServerWriter<ChatResponse> *writer, const std::string &model_id)
      : writer_(writer), model_id_(model_id) {}

protected:
  // This function is called when the buffer is full or flushed
  int overflow(int c) override;

  // This function is called when syncing/flushing
  int sync() override;

private:
  void flush_buffer();

  ServerWriter<ChatResponse> *writer_;
  std::string model_id_;
  std::string buffer_;
};

class InferenceServiceImpl final : public InferenceService::Service {
public:
  InferenceServiceImpl(const std::string &input_path, bool is_directory);
  ~InferenceServiceImpl() override;

  Status ChatCompletion(ServerContext *context, const ChatRequest *request,
                        ServerWriter<ChatResponse> *writer) override;

  Status Embeddings(ServerContext *context, const EmbeddingRequest *request,
                    EmbeddingResponse *response) override;

  Status ListModels(ServerContext *context, const ListModelsRequest *request,
                    ListModelsResponse *response) override;

private:
  // Returns pointer to loaded LLM, or nullptr if failed.
  // Loads on demand if not already loaded.
  MNN::Transformer::Llm *GetOrLoadModel(const std::string &model_id);
  void ScanModels();

  std::string input_path_;
  bool is_directory_;

  // model_id -> config_path
  std::map<std::string, std::string> available_models_;

  // model_id -> llm instance
  std::map<std::string, std::unique_ptr<MNN::Transformer::Llm>> loaded_models_;

  std::mutex model_mutex_; // Thread safety for loading
};

} // namespace inference
} // namespace tactorder

#endif // SERVICE_IMPL_H
