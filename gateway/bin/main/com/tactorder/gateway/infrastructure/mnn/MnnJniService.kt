package com.tactorder.gateway.infrastructure.mnn

import com.tactorder.gateway.domain.model.*
import com.tactorder.gateway.domain.port.InferenceService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.withContext
import org.slf4j.LoggerFactory
import java.util.UUID

class MnnJniService(
    private val nativeEngine: NativeEnginePort,
    private val modelPathResolver: (String) -> String
) : InferenceService {
    
    private val logger = LoggerFactory.getLogger(MnnJniService::class.java)

    override fun chatCompletion(request: ChatRequest): Flow<ChatResponse> {
        val modelPath = modelPathResolver(request.model)
        
        return if (request.stream) {
            callbackFlow {
                try {
                    withContext(Dispatchers.IO) {
                        // Ensure model is loaded (naive implementation: assumes single model for now or manages internally)
                        // In a real scenario, we might want a resource manager here.
                        nativeEngine.loadModel(modelPath)
                        
                        val prompt = request.messages.lastOrNull()?.content ?: ""
                        
                        nativeEngine.generateStringStream(prompt) { token ->
                            if (token.isNotEmpty()) {
                                val chunk = createChatResponse(request.model, token, null)
                                trySend(chunk)
                            }
                            true // continue
                        }
                        close()
                    }
                } catch (e: Exception) {
                    logger.error("Error in MNN native stream", e)
                    close(e)
                }
                awaitClose { /* Cleanup if needed */ }
            }.flowOn(Dispatchers.IO)
        } else {
            flow {
                val result = withContext(Dispatchers.IO) {
                    nativeEngine.loadModel(modelPath)
                    val prompt = request.messages.lastOrNull()?.content ?: ""
                    nativeEngine.generateString(prompt)
                }
                emit(createChatResponse(request.model, result, "stop"))
            }.flowOn(Dispatchers.IO)
        }
    }

    override suspend fun embeddings(request: EmbeddingRequest): EmbeddingResponse = withContext(Dispatchers.IO) {
        // Embeddings logic
         // Naive: assumes modelPathResolver handles embedding models too or we use a different prefix
        val modelPath = modelPathResolver(request.model)
        nativeEngine.loadEmbeddingModel(modelPath)

        val data = request.input.mapIndexed { index, text ->
            val floats = nativeEngine.embed(text) ?: floatArrayOf()
            EmbeddingData(
                objectType = "embedding",
                embedding = floats.toList(),
                index = index
            )
        }

        EmbeddingResponse(
            objectType = "list",
            data = data,
            model = request.model,
            usage = Usage(0, 0, 0)
        )
    }

    override suspend fun listModels(): List<String> {
        // TODO: Implement model discovery from filesystem
        return listOf("native-qwen2-0.5b-instruct", "native-bge-m3-quant")
    }

    private fun createChatResponse(model: String, content: String, finishReason: String?): ChatResponse {
        return ChatResponse(
            id = "native-" + UUID.randomUUID().toString(),
            created = System.currentTimeMillis() / 1000,
            model = model,
            choices = listOf(
                ChatChoice(
                    index = 0,
                    message = if (finishReason != null) ChatMessage("assistant", content) else null,
                    delta = if (finishReason == null) ChatMessage("assistant", content) else null,
                    finishReason = finishReason
                )
            ),
            usage = null
        )
    }
}
