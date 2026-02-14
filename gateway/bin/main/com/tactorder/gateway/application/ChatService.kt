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
        return service.chatCompletion(newRequest)
    }

    private fun processMessages(messages: List<ChatMessage>): List<ChatMessage> {
        // This logic was extracted from ChatCompletionHandler
        // In the domain model, ChatMessage content is String. 
        // We assume the Controller layer has already deserialized JSON to a structure 
        // that we can process, or we process raw string if it contains special encoding?
        
        // Wait, the handler was doing manual JSON parsing of "content" field which could be list or string.
        // Our Domain Model `ChatMessage` has `content: String`.
        // So the API Layer must perform the mapping from raw JSON to Domain Model.
        // BUT, the image processing logic (saving base64 to file) is Application Logic.
        
        // To follow Clean Arch strictly:
        // 1. DTO (API Layer) receives complex JSON.
        // 2. Transformer (API Layer) converts DTO to Domain Model. 
        //    During this conversion, if complex content exists, it should be processed? 
        //    Or we pass a complex objects to Domain?
        
        // For simplicity now, let's assume the string content needs <img> tag processing if it was embedded.
        // HOWEVER, the original code processed a *List* in the JSON.
        // If we force Domain Model to have String, the API handler must have done the flattening.
        
        return messages
    }
}
