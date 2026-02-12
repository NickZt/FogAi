#include "service_impl.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace tactorder {
namespace inference {

InferenceServiceImpl::InferenceServiceImpl() {
  // Initialize MNN engine here
  std::cout << "Inference Service Initialized" << std::endl;
}

InferenceServiceImpl::~InferenceServiceImpl() {}

Status
InferenceServiceImpl::ChatCompletion(ServerContext *context,
                                     const ChatRequest *request,
                                     ServerWriter<ChatResponse> *writer) {
  std::cout << "Received Chat Completion Request for model: "
            << request->model_id() << std::endl;

  // TODO: Integrate actual MNN inference here
  // For MVP, stream mock responses

  std::string mock_content[] = {"Hello",    " from",   " MNN",
                                " Service", " (mock)", "!"};

  for (const auto &token : mock_content) {
    if (context->IsCancelled()) {
      return Status::CANCELLED;
    }

    ChatResponse response;
    response.set_id("chatcmpl-mock-mnn");
    response.set_created(1234567890);
    response.set_model(request->model_id());

    auto *choice = response.add_choices();
    choice->set_index(0);
    auto *delta = choice->mutable_delta();
    delta->set_role("assistant"); // Only first chunk needs role, but standard
                                  // often sends it
    delta->set_content(token);

    writer->Write(response);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Send finish reason
  ChatResponse final_response;
  final_response.set_id("chatcmpl-mock-mnn");
  final_response.set_created(1234567890);
  final_response.set_model(request->model_id());
  auto *choice = final_response.add_choices();
  choice->set_index(0);
  choice->set_finish_reason("stop");
  choice->mutable_delta(); // empty delta
  writer->Write(final_response);

  return Status::OK;
}

Status InferenceServiceImpl::Embeddings(ServerContext *context,
                                        const EmbeddingRequest *request,
                                        EmbeddingResponse *response) {
  std::cout << "Received Embeddings Request for model: " << request->model_id()
            << std::endl;
  // Mock response
  response->set_object("list");
  response->set_model(request->model_id());

  auto *embedding = response->add_data();
  embedding->set_object("embedding");
  embedding->set_index(0);
  embedding->add_embedding(0.1f);
  embedding->add_embedding(0.2f);
  embedding->add_embedding(0.3f);

  return Status::OK;
}

} // namespace inference
} // namespace tactorder
