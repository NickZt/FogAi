#include "service_impl.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;

void RunServer(const std::string &config_path) {
  std::string server_address("0.0.0.0:50051");
  tactorder::inference::InferenceServiceImpl service(config_path);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char **argv) {
  // Default to the config.json inside the model directory
  std::string config_path = "/home/nickzt/Projects/LLM_campf/models_mnn/"
                            "qwen2-0.5b-instruct/config.json";
  if (argc > 1) {
    config_path = argv[1];
  }
  std::cout << "Starting MNN Inference Service with config: " << config_path
            << std::endl;
  RunServer(config_path);
  return 0;
}
