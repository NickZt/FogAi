#include "service_impl.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;

void RunServer(const std::string &model_path) {
  std::string server_address("0.0.0.0:50051");
  tactorder::inference::InferenceServiceImpl service(model_path);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char **argv) {
  std::string model_path =
      "/home/nickzt/Projects/LLM_campf/models_mnn/qwen2-0.5b-instruct/llm.mnn";
  if (argc > 1) {
    model_path = argv[1];
  }
  std::cout << "Starting MNN Inference Service with model: " << model_path
            << std::endl;
  RunServer(model_path);
  return 0;
}
