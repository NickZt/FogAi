package com.tactorder.gateway.application

import com.tactorder.gateway.domain.model.ChatRequest
import com.tactorder.gateway.domain.model.ChatResponse
import com.tactorder.gateway.domain.model.ChatMessage
import com.tactorder.gateway.router.RouterAi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.emptyFlow
import org.slf4j.LoggerFactory
import java.util.Base64
import java.io.File
import java.util.UUID

class ChatService(private val routerAi: RouterAi) {
    private val logger = LoggerFactory.getLogger(ChatService::class.java)

    /**
     * Orchestrates chat completion:
     * 1. Pre-process messages (handle images).
     * 2. Route to appropriate service.
     * 3. Call service.
     */
    suspend fun chatCompletion(request: ChatRequest): Flow<ChatResponse> {
        val processedMessages = processMessages(request.messages)
        val service = routerAi.getService(request.model)
        
        if (service == null) {
            logger.warn("No service found for model: ${request.model}")
            // Return flow that maybe emits an error or just empty? 
            // Better to throw so handler can return 404/500
            throw IllegalArgumentException("Model not found: ${request.model}")
        }
        
        val newRequest = request.copy(messages = processedMessages)
        
        val priority = if (request.model.contains("gliner", ignoreCase = true)) {
            com.tactorder.gateway.application.Priority.CRITICAL
        } else {
            com.tactorder.gateway.application.Priority.NORMAL
        }
        
        return kotlinx.coroutines.flow.flow {
            routerAi.queueManager.execute(priority) {
                service.chatCompletion(newRequest).collect { emit(it) }
            }
        }
    }

    private fun processMessages(messages: List<ChatMessage>): List<ChatMessage> {
        // This logic was extracted from ChatCompletionHandler
        // In the domain model, ChatMessage content is String. 
        // We assume the Controller layer has already deserialized JSON to a structure 
        // that we can process. TODO check or we process raw string if it contains special encoding?
        return messages
    }
}
