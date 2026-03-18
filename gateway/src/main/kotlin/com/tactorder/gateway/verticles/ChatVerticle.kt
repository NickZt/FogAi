package com.tactorder.gateway.verticles

import com.tactorder.gateway.application.Priority
import com.tactorder.gateway.domain.model.ChatRequest
import com.tactorder.gateway.domain.model.ChatMessage
import com.tactorder.gateway.router.RouterAi
import io.vertx.core.json.JsonObject
import io.vertx.core.json.JsonArray
import io.vertx.kotlin.coroutines.CoroutineVerticle
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.launch
import org.slf4j.LoggerFactory
import io.vertx.core.json.Json

class ChatVerticle : CoroutineVerticle() {
    private val logger = LoggerFactory.getLogger(ChatVerticle::class.java)
    private lateinit var routerAi: RouterAi

    override suspend fun start() {
        logger.info("Starting ChatVerticle...")
        // Initialize RouterAi only for models NOT containing 'embedding'
        routerAi = RouterAi(vertx) { modelId ->
            !modelId.contains("embedding", ignoreCase = true)
        }

        vertx.eventBus().consumer<String>("mnn.models.chat.request") { msg ->
            launch {
                try {
                    val models = routerAi.listModels()
                    msg.reply(JsonArray(models))
                } catch (e: Exception) {
                    msg.fail(500, e.message)
                }
            }
        }

        vertx.eventBus().consumer<JsonObject>("mnn.chat.request") { msg ->
            launch {
                val requestJson = msg.body()
                val isStream = requestJson.getBoolean("stream", false)
                val replyAddress = requestJson.getString("replyAddress")
                requestJson.remove("replyAddress")
                try {
                    val request = try {
                        requestJson.mapTo(ChatRequest::class.java)
                    } catch(e: Exception) {
                        logger.error("Failed to map request", e)
                        msg.reply(JsonObject().put("error", "Invalid request formulation"))
                        return@launch
                    }

                    val service = routerAi.getService(request.model)
                    if (service == null) {
                        msg.reply(JsonObject().put("not_found", true))
                        return@launch
                    }

                    val priority = if (request.model.contains("gliner", ignoreCase = true)) {
                        Priority.CRITICAL
                    } else {
                        Priority.NORMAL
                    }

                    if (request.stream) {
                        if (replyAddress == null) {
                            msg.reply(JsonObject().put("error", "Missing replyAddress for stream"))
                            return@launch
                        }

                        msg.reply(JsonObject().put("status", "started"))

                        val queue = routerAi.getQueueManager(service)
                        try {
                            queue.execute(priority) {
                                service.chatCompletion(request).collect { response ->
                                    val chunkObj = JsonObject(Json.encode(response))
                                    vertx.eventBus().send(replyAddress, JsonObject().put("chunk", chunkObj))
                                }
                            }
                            vertx.eventBus().send(replyAddress, JsonObject().put("done", true))
                        } catch (e: Exception) {
                            logger.error("Chat generation stream error", e)
                            vertx.eventBus().send(replyAddress, JsonObject().put("error", e.message ?: "Stream failure"))
                        }
                    } else {
                        val queue = routerAi.getQueueManager(service)
                        val responses = queue.execute(priority) {
                            service.chatCompletion(request).toList()
                        }
                        
                        if (responses.isNotEmpty()) {
                            val first = responses.first()
                            val isStreamChunks = first.choices?.firstOrNull()?.delta != null

                            val finalResponse = if (isStreamChunks) {
                                val fullContent = StringBuilder()
                                var role = "assistant"
                                var finishReason: String? = null
                                
                                responses.forEach { resp ->
                                    resp.choices.forEach { choice ->
                                        choice.delta?.content?.let { fullContent.append(it) }
                                        choice.delta?.role?.takeIf { it.isNotEmpty() }?.let { role = it }
                                        if (choice.finishReason != null) finishReason = choice.finishReason
                                    }
                                }
                                
                                com.tactorder.gateway.domain.model.ChatResponse(
                                    id = first.id,
                                    created = first.created,
                                    model = first.model,
                                    choices = listOf(
                                        com.tactorder.gateway.domain.model.ChatChoice(
                                            index = 0,
                                            message = ChatMessage(role = role, content = fullContent.toString()),
                                            delta = null,
                                            finishReason = finishReason
                                        )
                                    ),
                                    usage = responses.last().usage
                                )
                            } else {
                                responses.last()
                            }
                            
                            val responseObj = JsonObject(Json.encode(finalResponse))
                            msg.reply(JsonObject().put("response", responseObj))
                        } else {
                            msg.reply(JsonObject().put("error", "Empty response from engine"))
                        }
                    }
                } catch (e: Exception) {
                    logger.error("Chat request failed", e)
                    msg.reply(JsonObject().put("error", e.message ?: "Unknown error"))
                }
            }
        }
    }
}
