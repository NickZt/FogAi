#include "service_impl.h"
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

using grpc::Server;
using grpc::ServerBuilder;

void RunServer(const std::string &input_path, bool is_directory,
               const std::string &port) {
  std::string server_address("0.0.0.0:" + port);
  tactorder::inference::InferenceServiceImpl service(input_path, is_directory);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxReceiveMessageSize(50 * 1024 * 1024); // 50MB
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

#include <fstream>
#include <map>

std::map<std::string, std::string> LoadEnv(const std::string &env_path) {
  std::map<std::string, std::string> env_vars;
  std::ifstream file(env_path);
  if (!file.is_open())
    return env_vars;

  std::string line;
  while (std::getline(file, line)) {
    size_t comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }
    size_t assign_pos = line.find('=');
    if (assign_pos != std::string::npos) {
      std::string key = line.substr(0, assign_pos);
      std::string value = line.substr(assign_pos + 1);
      // Trim whitespace (simple implementation)
      key.erase(0, key.find_first_not_of(" \t"));
      key.erase(key.find_last_not_of(" \t") + 1);
      value.erase(0, value.find_first_not_of(" \t"));
      value.erase(value.find_last_not_of(" \t") + 1);
      env_vars[key] = value;
    }
  }
  return env_vars;
}

int main(int argc, char **argv) {
  std::string input_path;
  bool is_dir = false;
  std::string server_port = "50051";

  // Load .env
  auto env = LoadEnv(".env");
  if (env.count("MODELS_DIR")) {
    input_path = env["MODELS_DIR"];
  }
  if (env.count("PORT")) {
    server_port = env["PORT"];
  }

  // Command line args override .env
  if (argc > 1) {
    input_path = argv[1];
  }

  if (input_path.empty()) {
    std::cerr << "Error: No models directory specified in .env (MODELS_DIR) or "
                 "argument."
              << std::endl;
    return 1;
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

  std::cout << "Configured Port: " << server_port << std::endl;
  // Pass port to RunServer (need to update RunServer signature)
  RunServer(input_path, is_dir, server_port);
  return 0;
}
