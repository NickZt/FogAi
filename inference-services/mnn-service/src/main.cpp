#include "service_impl.h"
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

using grpc::Server;
using grpc::ServerBuilder;

void RunServer(const std::string &input_path, bool is_directory) {
  std::string server_address("0.0.0.0:50051");
  tactorder::inference::InferenceServiceImpl service(input_path, is_directory);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxReceiveMessageSize(50 * 1024 * 1024); // 50MB
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char **argv) {
  std::string input_path;
  bool is_dir = false;

  if (argc > 1) {
    input_path = argv[1];
  } else {
    // Default directory
    input_path = "/home/nickzt/Projects/LLM_campf/models_mnn";
  }

  if (fs::is_directory(input_path)) {
    is_dir = true;
    std::cout << "Starting MNN Inference Service in Directory Mode: "
              << input_path << std::endl;
  } else if (fs::exists(input_path)) {
    is_dir = false;
    std::cout << "Starting MNN Inference Service in Single File Mode: "
              << input_path << std::endl;
  } else {
    std::cerr << "Invalid path: " << input_path << std::endl;
    return 1;
  }

  RunServer(input_path, is_dir);
  return 0;
}
