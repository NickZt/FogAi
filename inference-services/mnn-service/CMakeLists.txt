cmake_minimum_required(VERSION 3.15)
project(mnn-inference-service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan integration (handled via CMAKE_TOOLCHAIN_FILE)
# No need for conanbuildinfo.cmake in Conan 2.0 with CMakeToolchain

# Find Protobuf and gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Fetch MNN
include(FetchContent)
FetchContent_Declare(
    MNN
    GIT_REPOSITORY https://github.com/alibaba/MNN.git
    GIT_TAG        2.8.0 # Use a stable tag
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE
)
# Disable MNN demo/test/tools to speed up build
set(MNN_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MNN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MNN_BUILD_TEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(MNN)

# Proto file generation
set(PROTO_SRC_DIR "${CMAKE_SOURCE_DIR}/../../proto")
set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

get_filename_component(proto_abs "${PROTO_SRC_DIR}/inference.proto" ABSOLUTE)

# Get gRPC plugin
if(TARGET gRPC::grpc_cpp_plugin)
    set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
    find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

message(STATUS "Using protoc: ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "Using grpc_cpp_plugin: ${GRPC_CPP_PLUGIN_EXECUTABLE}")

add_custom_command(
    OUTPUT "${PROTO_GEN_DIR}/inference.pb.cc" "${PROTO_GEN_DIR}/inference.pb.h" "${PROTO_GEN_DIR}/inference.grpc.pb.cc" "${PROTO_GEN_DIR}/inference.grpc.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out="${PROTO_GEN_DIR}" --cpp_out="${PROTO_GEN_DIR}" -I "${PROTO_SRC_DIR}" --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE} "inference.proto"
    DEPENDS "${proto_abs}"
)

add_library(inference_proto
    "${PROTO_GEN_DIR}/inference.pb.cc"
    "${PROTO_GEN_DIR}/inference.pb.h"
    "${PROTO_GEN_DIR}/inference.grpc.pb.cc"
    "${PROTO_GEN_DIR}/inference.grpc.pb.h"
)
target_link_libraries(inference_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
target_include_directories(inference_proto PUBLIC "${PROTO_GEN_DIR}")

# Main executable
add_executable(mnn-service src/main.cpp src/service_impl.cpp)
target_link_libraries(mnn-service PRIVATE inference_proto MNN)
target_include_directories(mnn-service PRIVATE src)
