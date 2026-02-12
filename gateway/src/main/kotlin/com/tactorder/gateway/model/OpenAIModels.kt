package com.tactorder.gateway.model

import com.fasterxml.jackson.annotation.JsonInclude
import com.fasterxml.jackson.annotation.JsonProperty

@JsonInclude(JsonInclude.Include.NON_NULL)
data class ChatCompletionRequest(
    val model: String,
    val messages: List<ChatMessage>,
    val temperature: Double? = 0.7,
    @JsonProperty("top_p") val topP: Double? = 1.0,
    val n: Int? = 1,
    val stream: Boolean? = false,
    val stop: Any? = null, // Can be string or array of strings
    @JsonProperty("max_tokens") val maxTokens: Int? = null,
    @JsonProperty("presence_penalty") val presencePenalty: Double? = 0.0,
    @JsonProperty("frequency_penalty") val frequencyPenalty: Double? = 0.0,
    val user: String? = null
)

data class ChatMessage(
    val role: String,
    val content: String,
    val name: String? = null
)

@JsonInclude(JsonInclude.Include.NON_NULL)
data class ChatCompletionResponse(
    val id: String,
    val `object`: String = "chat.completion",
    val created: Long,
    val model: String,
    val choices: List<ChatChoice>,
    val usage: Usage? = null
)

data class ChatChoice(
    val index: Int,
    val message: ChatMessage?,
    val delta: ChatMessage?, // For streaming
    @JsonProperty("finish_reason") val finishReason: String?
)

data class Usage(
    @JsonProperty("prompt_tokens") val promptTokens: Int,
    @JsonProperty("completion_tokens") val completionTokens: Int,
    @JsonProperty("total_tokens") val totalTokens: Int
)
