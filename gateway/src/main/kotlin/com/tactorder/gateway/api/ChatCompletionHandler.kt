package com.tactorder.gateway.api

import com.tactorder.gateway.domain.model.ChatRequest
import com.tactorder.gateway.domain.model.ChatMessage
import io.vertx.core.json.JsonObject
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import io.vertx.kotlin.coroutines.dispatcher
import org.slf4j.LoggerFactory
import io.vertx.core.Vertx
import java.util.UUID
import io.vertx.kotlin.coroutines.coAwait
import io.vertx.core.eventbus.DeliveryOptions

class ChatCompletionHandler(private val vertx: Vertx) {
    private val logger = LoggerFactory.getLogger(ChatCompletionHandler::class.java)

    fun handle(ctx: RoutingContext) {
        val body = ctx.body().asJsonObject()
        
        val request = try {
            mapToChatRequest(body)
        } catch (e: Exception) {
            logger.warn("Invalid request body", e)
            ctx.response().setStatusCode(400).end("Invalid JSON: ${e.message}")
            return
        }

        logger.info("Received request for model: ${request.model}")

        val scope = CoroutineScope(ctx.vertx().dispatcher())
        
        scope.launch {
            try {
                if (request.stream) {
                    ctx.response().isChunked = true
                    ctx.response().putHeader("Content-Type", "text/event-stream")
                    
                    val replyAddress = UUID.randomUUID().toString()
                    val requestJson = JsonObject.mapFrom(request).put("replyAddress", replyAddress)
                    
                    val consumer = vertx.eventBus().consumer<JsonObject>(replyAddress) { msg ->
                        val data = msg.body()
                        if (data.containsKey("done") && data.getBoolean("done")) {
                            ctx.response().write("data: [DONE]\n\n")
                            ctx.response().end()
                            msg.reply(JsonObject().put("status", "ok"))
                        } else if (data.containsKey("error")) {
                            logger.error("Error from verticle: ${data.getString("error")}")
                            if (!ctx.response().ended()) {
                                ctx.response().end()
                            }
                            msg.reply(JsonObject().put("status", "ok"))
                        } else {
                            val chunk = data.getJsonObject("chunk")
                            if (chunk != null) {
                                ctx.response().write("data: ${chunk.encode()}\n\n")
                            }
                            msg.reply(JsonObject().put("status", "ok"))
                        }
                    }

                    ctx.response().endHandler { consumer.unregister() }
                    ctx.response().exceptionHandler { consumer.unregister() }

                    val deliveryOptions = DeliveryOptions().setSendTimeout(300000) // 5 minutes
                    val reply = vertx.eventBus().request<JsonObject>("mnn.chat.request", requestJson, deliveryOptions).coAwait()
                    if (reply.body().getString("status") != "started") {
                        if (!ctx.response().ended()) {
                            ctx.response().setStatusCode(500).end("Failed to start stream: ${reply.body().getString("error")}")
                            consumer.unregister()
                        }
                    }
                } else {
                    val requestJson = JsonObject.mapFrom(request)
                    val deliveryOptions = DeliveryOptions().setSendTimeout(300000) // 5 minutes
                    val reply = vertx.eventBus().request<JsonObject>("mnn.chat.request", requestJson, deliveryOptions).coAwait()
                    val data = reply.body()
                    if (data.containsKey("error")) {
                        ctx.response().setStatusCode(500).end(data.getString("error"))
                    } else if (data.containsKey("not_found")) {
                        ctx.response().setStatusCode(404).end("Model not found")
                    } else if (data.containsKey("response")) {
                        ctx.json(data.getJsonObject("response"))
                    } else {
                        ctx.response().setStatusCode(500).end("Invalid response from verticle")
                    }
                }
            } catch (e: Exception) {
                logger.error("Error processing inference over EventBus", e)
                if (!ctx.response().ended()) {
                    ctx.response().setStatusCode(500).end(e.message)
                }
            }
        }
    }
    
    private fun mapToChatRequest(json: JsonObject): ChatRequest {
        val messages = json.getJsonArray("messages").map { 
            val obj = it as JsonObject
            val contentObj = obj.getValue("content")
            val contentStr = if (contentObj is String) contentObj else contentObj.toString()
            
            ChatMessage(
                role = obj.getString("role"),
                content = contentStr
            )
        }
        
        return ChatRequest(
            model = json.getString("model"),
            messages = messages,
            temperature = json.getDouble("temperature", 0.7),
            topP = json.getDouble("topP", 1.0),
            maxTokens = json.getInteger("maxTokens", 1024),
            stream = json.getBoolean("stream", false)
        )
    }
}
