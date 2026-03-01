cmake_minimum_required(VERSION 3.15)
project(onnx-inference-service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Conan generated files path
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ONNX Runtime (Find or Fetch?)
# Simplest is to assume it is installed or find_package works.
# If not, we might need to fetch.
# For now, let's assume system installed or user provided.
# User mentioned "why python? ... use c++ as in mnn-service".
# This implies they might expect us to handle dependencies.
# But ORT is large.
# Let's try find_package(OnnxRuntime).
find_package(OnnxRuntime QUIET)
if (NOT OnnxRuntime_FOUND)
    # Try finding in standard paths (manual)
    find_library(ORT_LIB onnxruntime)
    find_path(ORT_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime core/session)
    
    if (ORT_LIB AND ORT_INCLUDE_DIR)
        add_library(OnnxRuntime::OnnxRuntime UNKNOWN IMPORTED)
        set_target_properties(OnnxRuntime::OnnxRuntime PROPERTIES
            IMPORTED_LOCATION "${ORT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}"
        )
        message(STATUS "Found OnnxRuntime (Manual): ${ORT_LIB}")
    else()
        message(STATUS "OnnxRuntime not found. Fetching prebuilt binary...")
        include(FetchContent)
        FetchContent_Declare(
            onnxruntime_prebuilt
            URL https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz
        )
        FetchContent_MakeAvailable(onnxruntime_prebuilt)
        
        set(ORT_ROOT "${onnxruntime_prebuilt_SOURCE_DIR}")
        set(ORT_LIB "${ORT_ROOT}/lib/libonnxruntime.so")
        set(ORT_INCLUDE_DIR "${ORT_ROOT}/include")
        
        add_library(OnnxRuntime::OnnxRuntime UNKNOWN IMPORTED)
        set_target_properties(OnnxRuntime::OnnxRuntime PROPERTIES
            IMPORTED_LOCATION "${ORT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}"
        )
        
        # Also need to link .so.1.17.1 properly or set RPATH?
        # For simplicity, we assume .so exists.
        # We might need to install it or set LD_LIBRARY_PATH when running.
    endif()
endif()

add_subdirectory(../common common)

include_directories(
    src
)

add_executable(onnx-service src/main.cpp src/onnx_service_impl.cpp)
target_link_libraries(onnx-service PRIVATE inference_common OnnxRuntime::OnnxRuntime)
