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
# --- MNN Dependency (Submodule) ---
set(MNN_BUILD_LLM ON CACHE BOOL "Enable MNN LLM" FORCE)
set(MNN_OPENMP OFF CACHE BOOL "Disable OpenMP" FORCE)
set(MNN_AVX512 OFF CACHE BOOL "Disable AVX512 for stability" FORCE)
set(MNN_USE_THREAD_POOL ON CACHE BOOL "Use ThreadPool" FORCE)

# Check if submodule exists
message(STATUS "Checking for submodule at: ${CMAKE_CURRENT_SOURCE_DIRECTORY}/MNN/CMakeLists.txt")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIRECTORY}/MNN/CMakeLists.txt")
    message(WARNING "MNN submodule not found via check! But proceeding to try add_subdirectory...")
    # message(FATAL_ERROR "MNN submodule not found! Run 'git submodule update --init --recursive'")
endif()

add_subdirectory(MNN)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIRECTORY}/MNN/include
    ${CMAKE_CURRENT_SOURCE_DIRECTORY}/MNN/transformers/llm/engine/include
    ${CMAKE_CURRENT_SOURCE_DIRECTORY}/src
)

# FetchContent_MakeAvailable(MNN) - REMOVED

# Fetch nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

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
# Link MNN, MNN_Express, and llm libraries
# Link MNN, MNN_Express, and llm libraries
# Link MNN, MNN_Express, and llm libraries
# Link MNN, MNN_Express, llm, and json libraries
target_link_libraries(mnn-service PRIVATE inference_proto MNN MNN_Express llm nlohmann_json::nlohmann_json)
message(STATUS "Including MNN headers from: ${CMAKE_CURRENT_SOURCE_DIR}/MNN/include")
target_include_directories(mnn-service PUBLIC src "${CMAKE_CURRENT_SOURCE_DIR}/MNN/include" "${CMAKE_CURRENT_SOURCE_DIR}/MNN/transformers/llm/engine/include")
