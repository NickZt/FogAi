#include "onnx_service_impl.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
  std::string server_address("0.0.0.0:50052"); // Port 50052 for ONNX service

  // Load Env for models dir
  // Default to what user requested:
  // /home/nickzt/Projects/LLM_campf/models_onnx/ But we should try to read .env

  std::string models_dir = "/home/nickzt/Projects/LLM_campf/models_onnx/";
  const char *env_dir = std::getenv("ONNX_MODELS_DIR");
  if (env_dir) {
    models_dir = env_dir;
  }

  tactorder::inference::OnnxServiceImpl service(models_dir);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "ONNX Inference Service listening on " << server_address
            << std::endl;
  std::cout << "Models Dir: " << models_dir << std::endl;

  server->Wait();
}

int main(int argc, char **argv) {
  RunServer();
  return 0;
}
