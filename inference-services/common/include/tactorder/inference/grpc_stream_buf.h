#ifndef GRPC_STREAM_BUF_H
#define GRPC_STREAM_BUF_H

#include "inference.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <streambuf>
#include <string>

namespace tactorder {
namespace inference {

using grpc::ServerWriter;

// Custom stream buffer to redirect std::ostream to gRPC ServerWriter
class GrpcStreamBuf : public std::streambuf {
public:
  GrpcStreamBuf(ServerWriter<ChatResponse> *writer, const std::string &model_id)
      : writer_(writer), model_id_(model_id) {}

protected:
  // This function is called when the buffer is full or flushed
  int overflow(int c) override {
    if (c != EOF) {
      buffer_ += static_cast<char>(c);
    }
    return c;
  }

  // This function is called when syncing/flushing
  int sync() override {
    flush_buffer();
    return 0;
  }

private:
  void flush_buffer() {
    if (buffer_.empty())
      return;

    // Log progress every ~10 tokens/chunks to show activity without flooding
    token_count_++;
    if (token_count_ % 5 == 0) {
      std::cout << "." << std::flush;
    }

    ChatResponse response;
    // We might want to customize ID generation, but for now simple
    // concatenation is fine. Ideally this ID should be unique per chunk or per
    // stream? OpenAI format usually has same ID for the whole stream. Here we
    // regenerate it? The original code did: response.set_id("chatcmpl-" +
    // model_id_); Let's keep it consistent with MNN implementation for now.
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

  ServerWriter<ChatResponse> *writer_;
  std::string model_id_;
  std::string buffer_;
  int token_count_ = 0;
};

} // namespace inference
} // namespace tactorder

#endif // GRPC_STREAM_BUF_H
