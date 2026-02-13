package com.tactorder.gateway.native

/**
 * JNI Bridge to the MNN C++ Inference Engine.
 * This object allows zero-copy inference for critical path requests.
 */
object NativeEngine {
    init {
        // Load the shared library. Ensure 'libmnn_bridge.so' is in java.library.path
        try {
            System.loadLibrary("mnn_bridge")
        } catch (e: UnsatisfiedLinkError) {
            System.err.println("Failed to load mnn_bridge library: ${e.message}")
            // Fallback or explicit error handling can be added here
        }
    }

    /**
     * Loads the MNN model from the specified config path.
     * @param modelPath Absolute path to the config.json of the model.
     * @return true if loaded successfully, false otherwise.
     */
    external fun loadModel(modelPath: String): Boolean

    /**
     * Unloads the currently loaded model and frees native resources.
     * @return true if unloaded successfully.
     */
    external fun unloadModel(): Boolean

    /**
     * Zero-copy generation.
     *
     * @param inputIdsAddress Memory address of the input IDs buffer (int*).
     * @param inputSize Number of tokens in the user input.
     * @param outputBufferAddress Memory address of the output buffer (int*) where generated tokens will be written.
     * @param outputCapacity Maximum number of tokens the output buffer can hold.
     * @return Number of tokens actually generated and written to outputBuffer.
     */
    external fun generate(
        inputIdsAddress: Long,
        inputSize: Int,
        outputBufferAddress: Long,
        outputCapacity: Int
    ): Int

    /**
     * Convenience method for simple string-based generation (Testing/Debugging).
     * Not zero-copy.
     */
    external fun generateString(prompt: String): String
}
