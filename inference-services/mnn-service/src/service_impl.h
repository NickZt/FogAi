#pragma once

#include <grpcpp/grpcpp.h>
#include "inference.grpc.pb.h"

// Forward declaration for MNN Interpreter (to be added)
// #include <MNN/Interpreter.hpp>

using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;

namespace tactorder {
namespace inference {

class InferenceServiceImpl final : public InferenceService::Service {
public:
    InferenceServiceImpl();
    ~InferenceServiceImpl() override;

    Status ChatCompletion(ServerContext* context, 
                          const ChatRequest* request, 
                          ServerWriter<ChatResponse>* writer) override;

    Status Embeddings(ServerContext* context, 
                      const EmbeddingRequest* request, 
                      EmbeddingResponse* response) override;
};

} // namespace inference
} // namespace tactorder
