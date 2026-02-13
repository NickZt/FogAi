#include "service_impl.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace tactorder {
namespace inference {

// --- GrpcStreamBuf Implementation ---

int GrpcStreamBuf::overflow(int c) {
  if (c != EOF) {
    buffer_ += static_cast<char>(c);
    // Optional: Flush on newline or specific size to ensure responsiveness
    // But std::ostream usually handles buffering.
    // We rely on sync() which is called by flush() or endl.
  }
  return c;
}

int GrpcStreamBuf::sync() {
  flush_buffer();
  return 0;
}

void GrpcStreamBuf::flush_buffer() {
  if (buffer_.empty())
    return;

  ChatResponse response;
  // Use a fixed ID for the stream or generate one
  response.set_id("chatcmpl-" + model_id_);
  response.set_created(time(nullptr));
  response.set_model(model_id_);

  auto *choice = response.add_choices();
  choice->set_index(0);
  auto *delta = choice->mutable_delta();
  delta->set_content(buffer_);

  writer_->Write(response);
  buffer_.clear();
}

// --- InferenceServiceImpl Implementation ---

InferenceServiceImpl::InferenceServiceImpl(const std::string &model_config_path)
    : model_config_path_(model_config_path) {
  if (LoadModel()) {
    std::cout << "MNN Llm Initialized successfully from: " << model_config_path_
              << std::endl;
  } else {
    std::cerr << "Failed to initialize MNN Llm from: " << model_config_path_
              << std::endl;
  }
}

InferenceServiceImpl::~InferenceServiceImpl() = default;

bool InferenceServiceImpl::LoadModel() {
  try {
    // Create LLM instance from config.json
    llm_.reset(MNN::Transformer::Llm::createLLM(model_config_path_));

    if (!llm_) {
      std::cerr << "Llm::createLLM returned null." << std::endl;
      return false;
    }

    llm_->load();
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Exception loading model: " << e.what() << std::endl;
    return false;
  }
}

Status
InferenceServiceImpl::ChatCompletion(ServerContext *context,
                                     const ChatRequest *request,
                                     ServerWriter<ChatResponse> *writer) {
  std::cout << "Received Chat Completion Request for model: "
            << request->model_id() << std::endl;

  if (!llm_) {
    return Status(grpc::INTERNAL, "Model not loaded");
  }

  // Convert ChatRequest messages to MNN::Transformer::ChatMessages
  MNN::Transformer::ChatMessages messages;
  for (const auto &msg : request->messages()) {
    messages.emplace_back(msg.role(), msg.content());
  }

  // Create custom output stream for streaming response
  GrpcStreamBuf stream_buf(writer, request->model_id());
  std::ostream os(&stream_buf);

  // Generate response
  // response() blocks until generation is complete, but writes to os
  // progressively
  try {
    llm_->response(messages, &os);
  } catch (const std::exception &e) {
    std::cerr << "Error during inference: " << e.what() << std::endl;
    return Status(grpc::INTERNAL, "Inference failed: " + std::string(e.what()));
  }

  // Ensure final flush
  os.flush();

  // Send finish reason
  ChatResponse final_response;
  final_response.set_id("chatcmpl-" + request->model_id());
  final_response.set_created(time(nullptr));
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
  // Embeddings implementation remains mock/todo for now as Llm class focuses on
  // generation MNN::Transformer::Embedding class exists but we initialized Llm.
  // TODO: Support Embeddings
  return Status::OK;
}

} // namespace inference
} // namespace tactorder
