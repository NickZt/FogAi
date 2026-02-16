package com.tactorder.gateway.infrastructure.mnn

import com.tactorder.gateway.native.NativeEngine

/**
 * Wrapper for the static NativeEngine to allow for mocking and easier testing.
 */
interface NativeEnginePort {
    fun loadModel(modelPath: String): Boolean
    fun unloadModel(): Boolean
    fun generateString(prompt: String): String
    fun generateStringStream(prompt: String, callback: NativeEngine.TokenCallback): String
    
    fun loadEmbeddingModel(modelPath: String): Boolean
    fun unloadEmbeddingModel(): Boolean
    fun embed(input: String): FloatArray?
}

class NativeEngineWrapper : NativeEnginePort {
    override fun loadModel(modelPath: String) = NativeEngine.loadModel(modelPath)
    override fun unloadModel() = NativeEngine.unloadModel()
    override fun generateString(prompt: String) = NativeEngine.generateString(prompt)
    override fun generateStringStream(prompt: String, callback: NativeEngine.TokenCallback) = 
        NativeEngine.generateStringStream(prompt, callback)
        
    override fun loadEmbeddingModel(modelPath: String) = NativeEngine.loadEmbeddingModel(modelPath)
    override fun unloadEmbeddingModel() = NativeEngine.unloadEmbeddingModel()
    override fun embed(input: String) = NativeEngine.embed(input)
}
