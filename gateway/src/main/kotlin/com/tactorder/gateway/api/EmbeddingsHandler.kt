package com.tactorder.gateway.api

import com.tactorder.gateway.router.RouterAi
import io.vertx.core.json.Json
import io.vertx.core.json.JsonObject
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import io.vertx.kotlin.coroutines.dispatcher
import org.slf4j.LoggerFactory

class EmbeddingsHandler(private val routerAi: RouterAi) {
    private val logger = LoggerFactory.getLogger(EmbeddingsHandler::class.java)

    data class EmbeddingRequest(
        val model: String,
        val input: Any // String or List<String>
    )

    data class EmbeddingResponse(
        val objectType: String = "list",
        val data: List<EmbeddingData>,
        val model: String,
        val usage: Usage
    )

    data class EmbeddingData(
        val objectType: String = "embedding",
        val embedding: List<Float>,
        val index: Int
    )

    data class Usage(
        val prompt_tokens: Int,
        val total_tokens: Int
    )

    fun handle(ctx: RoutingContext) {
        val request = try {
            ctx.body().asPojo(EmbeddingRequest::class.java)
        } catch (e: Exception) {
            logger.warn("Invalid request body", e)
            ctx.response().setStatusCode(400).end("Invalid JSON")
            return
        }

        if (request.model.startsWith("native-")) {
            val modelId = request.model.removePrefix("native-")
            // HARDCODED PATH for testing based on user download
            val modelPath = "/home/nickzt/Projects/LLM_campf/models_mnn/${modelId}/config.json"
            
            logger.info("Routing to Native Embedding. Model: $modelId Path: $modelPath")

            val scope = CoroutineScope(ctx.vertx().dispatcher())
            scope.launch {
                try {
                    val embeddings = withContext(Dispatchers.IO) {
                        // Load model (simple check/load pattern)
                        val loadResult = com.tactorder.gateway.native.NativeEngine.loadEmbeddingModel(modelPath)
                        if (!loadResult) {
                            logger.warn("NativeEngine.loadEmbeddingModel returned false for $modelPath")
                            // Proceeding anyway? Or throw? MNN might return false if already loaded or failed.
                            // My C++ impl returns false if failed.
                            // But also resets if already loaded.
                        }
                        
                        val inputs = if (request.input is List<*>) {
                            request.input as List<String>
                        } else {
                            listOf(request.input.toString())
                        }
                        
                        inputs.mapIndexed { index, text ->
                            val floatArray = com.tactorder.gateway.native.NativeEngine.embed(text)
                            if (floatArray == null) {
                                throw RuntimeException("Embedding generation failed (returned null) for input: $text")
                            }
                            EmbeddingData(
                                objectType = "embedding",
                                embedding = floatArray.toList(),
                                index = index
                            )
                        }
                    }

                    // Constuct response using Jackson annotations or manual JSON
                    val response = EmbeddingResponse(
                        objectType = "list",
                        data = embeddings,
                        model = request.model,
                        usage = Usage(0, 0)
                    )
                    
                    ctx.vertx().runOnContext { 
                        ctx.json(response)
                    }
                    
                } catch (e: Exception) {
                    logger.error("Native embedding failed", e)
                    ctx.vertx().runOnContext { ctx.fail(500, e) }
                }
            }
            return
        }

        // Fallback or other router logic
        ctx.fail(501) // Not implemented for non-native yet
    }
}
