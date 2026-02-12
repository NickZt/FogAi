package com.tactorder.gateway.api

import com.tactorder.gateway.model.ChatCompletionRequest
import com.tactorder.gateway.model.ChatCompletionResponse
import com.tactorder.gateway.model.ChatChoice
import com.tactorder.gateway.model.ChatMessage
import com.tactorder.gateway.model.Usage
import com.tactorder.gateway.router.Router
import com.tactorder.inference.proto.ChatRequest
import com.tactorder.inference.proto.Message
import io.vertx.core.json.Json
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.launch
import io.vertx.kotlin.coroutines.dispatcher
import kotlinx.coroutines.CoroutineScope
import org.slf4j.LoggerFactory
import java.util.UUID

class ChatCompletionHandler(private val router: Router) {
    private val logger = LoggerFactory.getLogger(ChatCompletionHandler::class.java)

    fun handle(ctx: RoutingContext) {
        val request = try {
            ctx.body().asPojo(ChatCompletionRequest::class.java)
        } catch (e: Exception) {
            logger.warn("Invalid request body", e)
            ctx.response().setStatusCode(400).end("Invalid JSON: ${e.message}")
            return
        }

        logger.info("Received request for model: ${request.model}")

        val client = router.getClientForModel(request.model)
        if (client == null) {
            ctx.response().setStatusCode(404).end("Model not found")
            return
        }

        // Map to Proto
        val protoRequest = ChatRequest.newBuilder()
            .setModelId(request.model)
            .addAllMessages(request.messages.map { 
                Message.newBuilder().setRole(it.role).setContent(it.content).build() 
            })
            .setTemperature(request.temperature?.toFloat() ?: 0.7f)
            .setTopP(request.topP?.toFloat() ?: 1.0f)
            .setMaxTokens(request.maxTokens ?: 1024)
            .setStream(request.stream ?: false)
            .build()

        val scope = CoroutineScope(ctx.vertx().dispatcher())
        
        scope.launch {
            try {
                if (request.stream == true) {
                    // Streaming response
                    ctx.response().isChunked = true
                    ctx.response().putHeader("Content-Type", "text/event-stream")
                    
                    client.chatCompletionStream(protoRequest).collect { protoResp ->
                        // Convert to OpenAI chunk format
                        val choice = protoResp.choicesList.firstOrNull()
                        if (choice != null) {
                             val chunk = ChatCompletionResponse(
                                id = protoResp.id,
                                created = protoResp.created,
                                model = protoResp.model,
                                choices = listOf(
                                    com.tactorder.gateway.model.ChatChoice(
                                        index = choice.index,
                                        message = null,
                                        delta = com.tactorder.gateway.model.ChatMessage(
                                            role = choice.delta.role,
                                            content = choice.delta.content
                                        ),
                                        finishReason = if (choice.finishReason.isEmpty()) null else choice.finishReason
                                    )
                                )
                            )
                            ctx.response().write("data: ${Json.encode(chunk)}\n\n")
                        }
                    }
                    ctx.response().write("data: [DONE]\n\n")
                    ctx.response().end()
                } else {
                    // Non-streaming: accumulate
                    val responses = client.chatCompletionStream(protoRequest).toList()
                    val sb = StringBuilder()
                    responses.forEach { resp ->
                         resp.choicesList.forEach { c -> sb.append(c.delta.content) }
                    }
                    
                    // Simple aggregation for MVP
                    val fullResponse = ChatCompletionResponse(
                        id = UUID.randomUUID().toString(),
                        created = System.currentTimeMillis() / 1000,
                        model = request.model,
                        choices = listOf(
                            com.tactorder.gateway.model.ChatChoice(
                                index = 0,
                                message = com.tactorder.gateway.model.ChatMessage(
                                    role = if (sb.isNotEmpty() && responses.isNotEmpty() && responses[0].choicesList.isNotEmpty()) responses[0].choicesList[0].delta.role.ifEmpty { "assistant" } else "assistant", 
                                    content = sb.toString()
                                ),
                                delta = null,
                                finishReason = "stop"
                            )
                        ),
                        usage = Usage(0, 0, 0)
                    )
                    ctx.json(fullResponse)
                }
            } catch (e: Exception) {
                logger.error("Error processing inference", e)
                if (!ctx.response().ended()) {
                    ctx.response().setStatusCode(500).end(e.message)
                }
            }
        }
    }
}
