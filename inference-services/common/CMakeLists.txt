cmake_minimum_required(VERSION 3.15)
project(inference_common CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Fetch nlohmann/json if not found
find_package(nlohmann_json 3.11.2 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.2
    )
    FetchContent_MakeAvailable(json)
endif()

# --- Fetches (Double check if we need to duplicate this or if parent handles it) ---
# To be safe and self-contained, we can redeclare or assume parent project context.
# best practice for this structure: keep fetches in root or assume standard paths.
# For now, let's keep it simple and assume standard find_package works or we add logic.

# Proto Generation
set(PROTO_SRC_DIR "${CMAKE_SOURCE_DIR}/../../proto")
set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

get_filename_component(proto_abs "${PROTO_SRC_DIR}/inference.proto" ABSOLUTE)

if(TARGET gRPC::grpc_cpp_plugin)
    set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
    find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

add_custom_command(
    OUTPUT "${PROTO_GEN_DIR}/inference.pb.cc" "${PROTO_GEN_DIR}/inference.pb.h" "${PROTO_GEN_DIR}/inference.grpc.pb.cc" "${PROTO_GEN_DIR}/inference.grpc.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out="${PROTO_GEN_DIR}" --cpp_out="${PROTO_GEN_DIR}" -I "${PROTO_SRC_DIR}" --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE} "inference.proto"
    DEPENDS "${proto_abs}"
)

add_library(inference_common STATIC
    "${PROTO_GEN_DIR}/inference.pb.cc"
    "${PROTO_GEN_DIR}/inference.pb.h"
    "${PROTO_GEN_DIR}/inference.grpc.pb.cc"
    "${PROTO_GEN_DIR}/inference.grpc.pb.h"
    src/server_base.cpp
    src/tokenizer.cpp
    # src/grpc_stream_buf.cpp # If we split implementation
)

target_link_libraries(inference_common PUBLIC 
    protobuf::libprotobuf 
    gRPC::grpc++
    nlohmann_json::nlohmann_json
)

target_include_directories(inference_common PUBLIC 
    include
    "${PROTO_GEN_DIR}"
)

add_executable(test_tokenizer src/test_tokenizer.cpp)
target_link_libraries(test_tokenizer PRIVATE inference_common)
