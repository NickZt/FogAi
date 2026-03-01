package com.tactorder.gateway.api

import com.tactorder.gateway.application.ChatService
import com.tactorder.gateway.domain.model.ChatRequest
import com.tactorder.gateway.domain.model.ChatMessage
import com.tactorder.gateway.domain.model.ChatResponse
import com.tactorder.gateway.domain.model.ChatChoice
import com.tactorder.gateway.domain.model.Usage
import io.vertx.core.json.Json
import io.vertx.core.json.JsonObject
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.launch
import io.vertx.kotlin.coroutines.dispatcher
import org.slf4j.LoggerFactory
import kotlinx.coroutines.flow.collect

class ChatCompletionHandler(private val chatService: ChatService) {
    private val logger = LoggerFactory.getLogger(ChatCompletionHandler::class.java)

    fun handle(ctx: RoutingContext) {
        val body = ctx.body().asJsonObject()
        
        // Manual mapping from Vert.x JsonObject to Domain Model to handle flexibility
        // (Fast mapping for now, ideally TODO use Jackson with custom deserializer coverage)
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
                    
                    chatService.chatCompletion(request).collect { response ->
                         val json = Json.encode(response)
                         ctx.response().write("data: $json\n\n")
                    }
                    
                    ctx.response().write("data: [DONE]\n\n")
                    ctx.response().end()
                } else {
                    val responses = chatService.chatCompletion(request).toList()
                    
                    if (responses.isNotEmpty()) {
                        // Check if we need to aggregate (if first response has delta, it's a stream chunk)
                        val first = responses.first()
                        val isStreamChunks = first.choices?.firstOrNull()?.delta != null
                        
                        if (isStreamChunks) {
                            // Aggregate chunks
                            val fullContent = StringBuilder()
                            var role = "assistant" // Default
                            var finishReason: String? = null
                            
                            // Use first chunk for metadata
                            val template = first
                            
                            responses.forEach { resp ->
                                resp.choices.forEach { choice ->
                                    choice.delta?.content?.let { fullContent.append(it) }
                                    choice.delta?.role?.takeIf { it.isNotEmpty() }?.let { role = it }
                                    // Update finish reason if present
                                    if (choice.finishReason != null) {
                                        finishReason = choice.finishReason
                                    }
                                }
                            }
                            
                            // Construct complete structure
                            val completeMessage = ChatMessage(role = role, content = fullContent.toString())
                            
                            val aggregatedChoice = ChatChoice(
                                index = 0,
                                message = completeMessage,
                                delta = null,
                                finishReason = finishReason
                            )
                            
                            val aggregatedResponse = ChatResponse(
                                id = template.id,
                                created = template.created,
                                model = template.model,
                                choices = listOf(aggregatedChoice),
                                usage = responses.last().usage // Best effort usage
                            )
                            
                            ctx.json(aggregatedResponse)
                        } else {
                            // Already a single complete response (like MNN JNI might be)
                            ctx.json(responses.last())
                        }
                    } else {
                        ctx.fail(500)
                    }
                }
            } catch (e: IllegalArgumentException) {
                ctx.response().setStatusCode(404).end(e.message)
            } catch (e: Exception) {
                logger.error("Error processing inference", e)
                if (!ctx.response().ended()) {
                    ctx.response().setStatusCode(500).end(e.message)
                }
            }
        }
    }
    
    private fun mapToChatRequest(json: JsonObject): ChatRequest {
        val messages = json.getJsonArray("messages").map { 
            val obj = it as JsonObject
            // Handle content parsing separately if it's a list (for images)
            // For now, we assume simple string or pre-processed. 
            // Real logic needs to flatten separate content parts into one string or handling structure.
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
