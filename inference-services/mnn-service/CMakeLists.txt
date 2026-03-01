cmake_minimum_required(VERSION 3.15)
project(mnn-inference-service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Conan generated files path
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# Find Protobuf and gRPC (from Conan)
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Fetch MNN
include(FetchContent)
# --- MNN Dependency (Submodule) ---
set(MNN_BUILD_LLM ON CACHE BOOL "Enable MNN LLM" FORCE)
set(MNN_OPENMP OFF CACHE BOOL "Disable OpenMP" FORCE)
set(MNN_AVX512 ON CACHE BOOL "Enable AVX512 for performance" FORCE)
set(MNN_USE_THREAD_POOL ON CACHE BOOL "Use ThreadPool" FORCE)

# Check if submodule exists
message(STATUS "Checking for submodule at: ${CMAKE_CURRENT_SOURCE_DIR}/MNN/CMakeLists.txt")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MNN/CMakeLists.txt")
    message(WARNING "MNN submodule not found via check! But proceeding to try add_subdirectory...")
endif()

add_subdirectory(MNN)
add_subdirectory(../common common)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/MNN/include
    ${CMAKE_CURRENT_SOURCE_DIR}/MNN/transformers/llm/engine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- JNI Bridge Library ---
find_package(JNI REQUIRED)

add_library(mnn_bridge SHARED src/mnn_bridge.cpp)

# Link MNN, MNN_Express, llm, and JNI
target_link_libraries(mnn_bridge PRIVATE MNN MNN_Express llm JNI::JNI)

target_include_directories(mnn_bridge PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/MNN/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/MNN/transformers/llm/engine/include"
    "${JNI_INCLUDE_DIRS}"
)

# Main executable
add_executable(mnn-service src/main.cpp src/service_impl.cpp)
# Link MNN, MNN_Express, llm, and common library (which has proto and json)
target_link_libraries(mnn-service PRIVATE inference_common MNN MNN_Express llm protobuf::libprotobuf gRPC::grpc++ gRPC::grpc nlohmann_json::nlohmann_json)
message(STATUS "Including MNN headers from: ${CMAKE_CURRENT_SOURCE_DIR}/MNN/include")
target_include_directories(mnn-service PUBLIC src 
    "${CMAKE_CURRENT_SOURCE_DIR}/MNN/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/MNN/transformers/llm/engine/include"
)
