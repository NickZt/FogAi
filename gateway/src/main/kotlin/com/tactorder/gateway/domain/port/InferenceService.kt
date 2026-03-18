package com.tactorder.gateway.domain.port

import com.tactorder.gateway.domain.model.ChatRequest
import com.tactorder.gateway.domain.model.ChatResponse
import com.tactorder.gateway.domain.model.EmbeddingRequest
import com.tactorder.gateway.domain.model.EmbeddingResponse
import kotlinx.coroutines.flow.Flow

interface InferenceService {
    /**
     * Supports both streaming and non-streaming via Flow.
     * For non-streaming, the Flow will emit a single item or accumulated items.
     */
    fun chatCompletion(request: ChatRequest): Flow<ChatResponse>

    suspend fun embeddings(request: EmbeddingRequest): EmbeddingResponse
    
    suspend fun listModels(): List<String>
    
    suspend fun isHealthy(): Boolean
}
