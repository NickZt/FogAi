message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(gRPC)
find_package(protobuf)
find_package(nlohmann_json)

set(CONANDEPS_LEGACY  grpc::grpc  protobuf::protobuf  nlohmann_json::nlohmann_json )