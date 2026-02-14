package com.tactorder.gateway.domain.model

data class ChatMessage(
    val role: String,
    val content: String, // Simplified for internal use, image tags already processed
    val name: String? = null
)

data class ChatRequest(
    val model: String,
    val messages: List<ChatMessage>,
    val temperature: Double = 0.7,
    val topP: Double = 1.0,
    val maxTokens: Int = 1024,
    val stream: Boolean = false
)

data class ChatResponse(
    val id: String,
    val created: Long,
    val model: String,
    val choices: List<ChatChoice>,
    val usage: Usage? = null
)

data class ChatChoice(
    val index: Int,
    val message: ChatMessage?,
    val delta: ChatMessage?,
    val finishReason: String?
)

data class Usage(
    val promptTokens: Int,
    val completionTokens: Int,
    val totalTokens: Int
)

data class EmbeddingRequest(
    val model: String,
    val input: List<String>
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
