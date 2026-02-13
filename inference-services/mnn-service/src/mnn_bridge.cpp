#include "llm/llm.hpp"
#include <cstring>
#include <iostream>
#include <jni.h>
#include <memory>
#include <string>

// Global LLM instance
std::unique_ptr<MNN::Transformer::Llm> g_llm_instance = nullptr;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_tactorder_gateway_native_NativeEngine_loadModel(JNIEnv *env,
                                                         jobject thiz,
                                                         jstring modelPath) {
  const char *path = env->GetStringUTFChars(modelPath, nullptr);
  if (path == nullptr)
    return JNI_FALSE;

  std::string config_path(path);
  env->ReleaseStringUTFChars(modelPath, path);

  try {
    // Reset if already loaded
    if (g_llm_instance) {
      g_llm_instance.reset();
    }

    auto get_mem_available = []() -> float {
      FILE *fp = fopen("/proc/meminfo", "r");
      if (!fp)
        return -1.0f;
      char line[256];
      float mem_avail = -1.0f;
      while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemAvailable: %f kB", &mem_avail) == 1) {
          mem_avail /= 1024.0f; // MB
          break;
        }
      }
      fclose(fp);
      return mem_avail;
    };

    float mem_before = get_mem_available();
    std::cout << "[NativeBridge] Loading model from: " << config_path
              << std::endl;
    if (mem_before > 0)
      std::cout << "[NativeBridge] MemAvailable Before: " << mem_before << " MB"
                << std::endl;

    g_llm_instance.reset(MNN::Transformer::Llm::createLLM(config_path.c_str()));

    if (!g_llm_instance) {
      std::cerr << "[NativeBridge] Failed to create LLM instance." << std::endl;
      return JNI_FALSE;
    }

    g_llm_instance->load();

    float mem_after = get_mem_available();
    std::cout << "[NativeBridge] Model loaded successfully." << std::endl;
    if (mem_after > 0)
      std::cout << "[NativeBridge] MemAvailable After: " << mem_after
                << " MB (Diff: " << (mem_before - mem_after) << " MB)"
                << std::endl;

    return JNI_TRUE;
  } catch (const std::exception &e) {
    std::cerr << "[NativeBridge] Exception loading model: " << e.what()
              << std::endl;
    return JNI_FALSE;
  }
}

JNIEXPORT jboolean JNICALL
Java_com_tactorder_gateway_native_NativeEngine_unloadModel(JNIEnv *env,
                                                           jobject thiz) {
  try {
    if (g_llm_instance) {
      std::cout << "[NativeBridge] Unloading model..." << std::endl;
      g_llm_instance.reset(); // Effectively unloads
      std::cout << "[NativeBridge] Model unloaded." << std::endl;
      return JNI_TRUE;
    }
    return JNI_FALSE;
  } catch (...) {
    return JNI_FALSE;
  }
}

JNIEXPORT jint JNICALL Java_com_tactorder_gateway_native_NativeEngine_generate(
    JNIEnv *env, jobject thiz, jlong inputIdsAddress, jint inputSize,
    jlong outputBufferAddress, jint outputCapacity) {
  if (!g_llm_instance)
    return -1;

  // Zero-copy access via raw pointers
  // Requires that the JVM caller allocated direct byte buffers or similar and
  // passed the address
  const int *input_ids = reinterpret_cast<const int *>(inputIdsAddress);
  int *output_buffer = reinterpret_cast<int *>(outputBufferAddress);

  if (!input_ids || !output_buffer)
    return -1;

  try {
    std::vector<int> input_vector(input_ids, input_ids + inputSize);

    // This is a simplified synchronous generation for now
    // In reality, MNN generation is stream-based or returns a vector.
    // We will collect the result into a vector and copy to the buffer.

    // Using response() writes to ostream, generate() returns vector
    std::vector<int> output_tokens =
        g_llm_instance->generate(input_vector, outputCapacity);

    int count = 0;
    for (int token : output_tokens) {
      if (count >= outputCapacity)
        break;
      output_buffer[count++] = token;
    }

    return count;
  } catch (const std::exception &e) {
    std::cerr << "[NativeBridge] Generation error: " << e.what() << std::endl;
    return -1;
  }
}

JNIEXPORT jstring JNICALL
Java_com_tactorder_gateway_native_NativeEngine_generateString(JNIEnv *env,
                                                              jobject thiz,
                                                              jstring prompt) {
  if (!g_llm_instance)
    return env->NewStringUTF("Error: Model not loaded");

  const char *prompt_cstr = env->GetStringUTFChars(prompt, nullptr);
  if (!prompt_cstr)
    return nullptr;

  std::string prompt_str(prompt_cstr);
  env->ReleaseStringUTFChars(prompt, prompt_cstr);

  try {
    std::stringstream ss;
    // Using response which handles chat template automatically if prompt is raw
    // text or we might need to tokenizer_encode first. For simplicity, let's
    // use response() dumping to stringstream.
    g_llm_instance->response(prompt_str, &ss);

    return env->NewStringUTF(ss.str().c_str());
  } catch (const std::exception &e) {
    std::string err = "Error: ";
    err += e.what();
    return env->NewStringUTF(err.c_str());
  }
}

} // extern "C"
