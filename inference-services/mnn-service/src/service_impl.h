#ifndef SERVICE_IMPL_H
#define SERVICE_IMPL_H

#include "inference.grpc.pb.h"
#include <MNN/Interpreter.hpp>
#include <MNN/expr/Expr.hpp>
#include <MNN/expr/MathOp.hpp>
#include <MNN/expr/Module.hpp>
#include <MNN/expr/NeuralNetWorkOp.hpp>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <vector>

namespace tactorder {
namespace inference {

using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

class InferenceServiceImpl final : public InferenceService::Service {
public:
  InferenceServiceImpl(const std::string &model_path); // Modified constructor
  ~InferenceServiceImpl() override;

  Status ChatCompletion(ServerContext *context, const ChatRequest *request,
                        ServerWriter<ChatResponse> *writer) override;

  Status Embeddings(ServerContext *context, const EmbeddingRequest *request,
                    EmbeddingResponse *response) override;

private:
  bool LoadModel();

  std::string model_path_;
  std::shared_ptr<MNN::Express::Module> llm_module_;
};

} // namespace inference
} // namespace tactorder

#endif // SERVICE_IMPL_H
