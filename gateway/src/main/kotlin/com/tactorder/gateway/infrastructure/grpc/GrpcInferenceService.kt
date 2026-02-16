package com.tactorder.gateway.infrastructure.grpc

import com.tactorder.gateway.client.InferenceClient
import com.tactorder.gateway.domain.model.*
import com.tactorder.gateway.domain.port.InferenceService
import com.tactorder.inference.proto.ChatRequest as ProtoChatRequest
import com.tactorder.inference.proto.Message as ProtoMessage
import com.tactorder.inference.proto.EmbeddingRequest as ProtoEmbeddingRequest
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

class GrpcInferenceService(
    private val client: InferenceClient,
    private val modelPrefix: String = ""
) : InferenceService {

    private fun stripPrefix(modelId: String): String {
        return if (modelPrefix.isNotEmpty() && modelId.startsWith(modelPrefix)) {
            modelId.removePrefix(modelPrefix)
        } else {
            modelId
        }
    }

    override fun chatCompletion(request: ChatRequest): Flow<ChatResponse> {
        val targetModelId = stripPrefix(request.model)
        val protoRequest = ProtoChatRequest.newBuilder()
            .setModelId(targetModelId)
            .addAllMessages(request.messages.map { 
                ProtoMessage.newBuilder()
                    .setRole(it.role)
                    .setContent(it.content)
                    .build() 
            })
            .setTemperature(request.temperature.toFloat())
            .setTopP(request.topP.toFloat())
            .setMaxTokens(request.maxTokens)
            .setStream(request.stream)
            .build()
            
        return client.chatCompletionStream(protoRequest).map { protoResp ->
            // Map Proto response to Domain response
            val choice = protoResp.choicesList.firstOrNull()
            ChatResponse(
                id = protoResp.id,
                created = protoResp.created,
                // Let's prepend prefix if configured.
                model = if (modelPrefix.isNotEmpty()) modelPrefix + protoResp.model else protoResp.model,
                choices = listOf(
                    ChatChoice(
                        index = choice?.index ?: 0,
                        message = null, // Stream usually has delta
                        delta = if (choice != null) ChatMessage(choice.delta.role, choice.delta.content) else null,
                        finishReason = if (choice?.finishReason.isNullOrEmpty()) null else choice?.finishReason
                    )
                ),
                usage = null
            )
        }
    }

    override suspend fun embeddings(request: EmbeddingRequest): EmbeddingResponse {
        val targetModelId = stripPrefix(request.model)
        val protoReq = ProtoEmbeddingRequest.newBuilder()
            .setModelId(targetModelId)
            .addAllInput(request.input)
            .build()
            
        val protoResp = client.embeddings(protoReq)
        
        return EmbeddingResponse(
            objectType = protoResp.`object`, // Mapped to 'object' field in proto
            data = protoResp.dataList.map { 
                EmbeddingData(
                    objectType = it.`object`,
                    embedding = it.embeddingList,
                    index = it.index
                )
            },
            model = if (modelPrefix.isNotEmpty()) modelPrefix + protoResp.model else protoResp.model,
            usage = Usage(protoResp.usage.promptTokens, 0, protoResp.usage.totalTokens)
        )
    }
    
    override suspend fun listModels(): List<String> {
        return client.listModels().modelsList.map { it.id }
    }
}
