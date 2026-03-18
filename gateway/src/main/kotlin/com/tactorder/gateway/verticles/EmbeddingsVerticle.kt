package com.tactorder.gateway.verticles

import com.tactorder.gateway.application.Priority
import com.tactorder.gateway.domain.model.EmbeddingRequest
import com.tactorder.gateway.router.RouterAi
import io.vertx.core.json.JsonObject
import io.vertx.core.json.JsonArray
import io.vertx.kotlin.coroutines.CoroutineVerticle
import kotlinx.coroutines.launch
import org.slf4j.LoggerFactory
import io.vertx.core.json.Json

class EmbeddingsVerticle : CoroutineVerticle() {
    private val logger = LoggerFactory.getLogger(EmbeddingsVerticle::class.java)
    private lateinit var routerAi: RouterAi

    override suspend fun start() {
        logger.info("Starting EmbeddingsVerticle...")
        // Only models containing 'embedding' or 'gliner'
        routerAi = RouterAi(vertx) { modelId ->
            modelId.contains("embedding", ignoreCase = true) || modelId.contains("gliner", ignoreCase = true)
        }

        vertx.eventBus().consumer<String>("mnn.models.embeddings.request") { msg ->
            launch {
                try {
                    val models = routerAi.listModels()
                    msg.reply(JsonArray(models))
                } catch (e: Exception) {
                    msg.fail(500, e.message)
                }
            }
        }

        vertx.eventBus().consumer<JsonObject>("mnn.embeddings.request") { msg ->
            launch {
                val requestJson = msg.body()
                try {
                    val request = try {
                        requestJson.mapTo(EmbeddingRequest::class.java)
                    } catch(e: Exception) {
                        msg.reply(JsonObject().put("error", "Invalid embedding request formulation"))
                        return@launch
                    }

                    val service = routerAi.getService(request.model)
                    if (service == null) {
                        msg.reply(JsonObject().put("not_found", true))
                        return@launch
                    }

                    val queue = routerAi.getQueueManager(service)
                    val response = queue.execute(Priority.BACKGROUND) {
                        service.embeddings(request)
                    }
                    
                    val responseObj = JsonObject(Json.encode(response))
                    msg.reply(JsonObject().put("response", responseObj))
                } catch (e: Exception) {
                    logger.error("Embeddings request failed", e)
                    msg.reply(JsonObject().put("error", e.message ?: "Unknown error"))
                }
            }
        }
    }
}
