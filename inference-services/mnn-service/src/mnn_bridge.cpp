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

// Custom StreamBuf to intercept output and call Java callback
class CallbackStreamBuf : public std::streambuf {
public:
  CallbackStreamBuf(JNIEnv *env, jobject callback, jmethodID onTokenMethod)
      : env_(env), callback_(callback), onTokenMethod_(onTokenMethod) {}

protected:
  // Called when the buffer is full or flushed
  virtual int_type overflow(int_type c) override {
    if (c != EOF) {
      char ch = static_cast<char>(c);
      buffer_ += ch;
    }
    return c;
  }

  // Called on std::flush or endl
  virtual int sync() override {
    if (!buffer_.empty()) {
      jstring jchunk = env_->NewStringUTF(buffer_.c_str());
      // Check for error in creation
      if (jchunk) {
        jboolean continueGen =
            env_->CallBooleanMethod(callback_, onTokenMethod_, jchunk);
        env_->DeleteLocalRef(jchunk);

        // TODO: Handle continueGen == false to stop generation
        // MNN LLM doesn't easily support external stop signal via stream,
        // passing continueGen back is tricky without modifying MNN.
      }
      full_response_ += buffer_;
      buffer_.clear();
    }
    return 0;
  }

public:
  std::string getFullResponse() const { return full_response_; }

private:
  JNIEnv *env_;
  jobject callback_;
  jmethodID onTokenMethod_;
  std::string buffer_;
  std::string full_response_;
};

JNIEXPORT jstring JNICALL
Java_com_tactorder_gateway_native_NativeEngine_generateStringStream(
    JNIEnv *env, jobject thiz, jstring prompt, jobject callback) {
  if (!g_llm_instance)
    return env->NewStringUTF("Error: Model not loaded");

  const char *prompt_cstr = env->GetStringUTFChars(prompt, nullptr);
  if (!prompt_cstr)
    return nullptr;
  std::string prompt_str(prompt_cstr);
  env->ReleaseStringUTFChars(prompt, prompt_cstr);

  // Trace Logging
  std::cout << "[NativeBridge] [TRACE] Input Type: Text" << std::endl;
  std::cout << "[NativeBridge] [TRACE] Processing Prompt: " << prompt_str
            << std::endl;

  // Get callback method ID
  jclass callbackClass = env->GetObjectClass(callback);
  jmethodID onToken =
      env->GetMethodID(callbackClass, "onToken", "(Ljava/lang/String;)Z");
  if (!onToken) {
    return env->NewStringUTF("Error: Method onToken not found");
  }

  try {
    CallbackStreamBuf buf(env, callback, onToken);
    std::ostream os(&buf);

    g_llm_instance->response(prompt_str, &os);
    os.flush(); // Ensure last bit is sent

    // Log Performance Metrics
    auto context = g_llm_instance->getContext();
    if (context) {
      int prompt_tokens = context->prompt_len;
      int generated_tokens = context->gen_seq_len;
      float prefill_time = context->prefill_us / 1000.0f; // ms
      float decode_time = context->decode_us / 1000.0f;   // ms
      float total_time = prefill_time + decode_time;
      float speed = 0.0f;
      if (decode_time > 0) {
        speed = generated_tokens / (decode_time / 1000.0f);
      }

      std::cout << "[Metrics] Prompt Tokens: " << prompt_tokens << std::endl;
      std::cout << "[Metrics] Generated Tokens: " << generated_tokens
                << std::endl;
      std::cout << "[Metrics] Prefill Time: " << prefill_time << " ms"
                << std::endl;
      std::cout << "[Metrics] Decode Time: " << decode_time << " ms"
                << std::endl;
      std::cout << "[Metrics] Speed: " << speed << " tokens/sec" << std::endl;
    }

    return env->NewStringUTF(buf.getFullResponse().c_str());
  } catch (const std::exception &e) {
    std::string err = "Error: ";
    err += e.what();
    return env->NewStringUTF(err.c_str());
  }
}

} // extern "C"

// ----------------------------------------------------------------------------
// Embedding Implementation
// ----------------------------------------------------------------------------

std::unique_ptr<MNN::Transformer::Embedding> g_embedding_instance = nullptr;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_tactorder_gateway_native_NativeEngine_loadEmbeddingModel(
    JNIEnv *env, jobject thiz, jstring modelPath) {
  const char *path = env->GetStringUTFChars(modelPath, nullptr);
  if (path == nullptr)
    return JNI_FALSE;

  std::string config_path(path);
  env->ReleaseStringUTFChars(modelPath, path);

  fprintf(stderr,
          "[NativeBridge] NativeEngine_loadEmbeddingModel called with: %s\n",
          config_path.c_str());
  fflush(stderr);

  try {
    if (g_embedding_instance) {
      g_embedding_instance.reset();
    }

    std::cout << "[NativeBridge] Loading embedding model from: " << config_path
              << std::endl;

    // createEmbedding(path, load=true)
    g_embedding_instance.reset(
        MNN::Transformer::Embedding::createEmbedding(config_path, true));

    if (!g_embedding_instance) {
      std::cerr << "[NativeBridge] Failed to create Embedding instance."
                << std::endl;
      return JNI_FALSE;
    }

    // load() is usually called by createEmbedding if load=true, but let's check
    // or we can call load() explicitly if needed. definition says "load = true"
    // default.
    std::cout << "[NativeBridge] Embedding model loaded." << std::endl;
    return JNI_TRUE;
  } catch (const std::exception &e) {
    std::cerr << "[NativeBridge] Exception loading embedding model: "
              << e.what() << std::endl;
    return JNI_FALSE;
  }
}

JNIEXPORT jboolean JNICALL
Java_com_tactorder_gateway_native_NativeEngine_unloadEmbeddingModel(
    JNIEnv *env, jobject thiz) {
  try {
    if (g_embedding_instance) {
      g_embedding_instance.reset();
      std::cout << "[NativeBridge] Embedding model unloaded." << std::endl;
      return JNI_TRUE;
    }
    return JNI_FALSE;
  } catch (...) {
    return JNI_FALSE;
  }
}

JNIEXPORT jfloatArray JNICALL
Java_com_tactorder_gateway_native_NativeEngine_embed(JNIEnv *env, jobject thiz,
                                                     jstring input) {
  if (!g_embedding_instance) {
    return nullptr;
  }

  const char *input_cstr = env->GetStringUTFChars(input, nullptr);
  if (!input_cstr)
    return nullptr;

  std::string input_str(input_cstr);
  env->ReleaseStringUTFChars(input, input_cstr);

  try {
    // Generate embedding
    // txt_embedding returns a VARP (Variable)
    auto var = g_embedding_instance->txt_embedding(input_str);

    if (var == nullptr) {
      std::cerr << "[NativeBridge] txt_embedding returned null VARP"
                << std::endl;
      return nullptr;
    }

    // Read data from VARP
    // We assume it returns a 1D or 2D tensor. For a single string, likely [1,
    // dim] or [dim].
    auto info = var->getInfo();
    if (!info) {
      // Need to ensure it's computed? VARP usually is lazy?
      // txt_embedding implementation usually runs forward.
      // Let's read it safely.
    }

    // readMap() ensures content is available on host
    const float *ptr = var->readMap<float>();
    if (!ptr) {
      std::cerr << "[NativeBridge] Failed to read embedding data" << std::endl;
      return nullptr;
    }

    int size = info->size;

    jfloatArray result = env->NewFloatArray(size);
    if (result == nullptr)
      return nullptr;

    env->SetFloatArrayRegion(result, 0, size, ptr);

    return result;
  } catch (const std::exception &e) {
    std::cerr << "[NativeBridge] Embedding error: " << e.what() << std::endl;
    return nullptr;
  }
}

} // extern "C"
