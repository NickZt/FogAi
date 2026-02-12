package com.tactorder.gateway.client

import com.tactorder.inference.proto.ChatRequest
import com.tactorder.inference.proto.ChatResponse
import com.tactorder.inference.proto.VertxInferenceServiceGrpc
import io.vertx.core.Vertx
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.channels.awaitClose

class InferenceClient(private val vertx: Vertx, private val host: String, private val port: Int) {
    
    private val stub: VertxInferenceServiceGrpc.InferenceServiceVertxStub

    init {
        val channel = io.vertx.grpc.VertxChannelBuilder
            .forAddress(vertx, host, port)
            .usePlaintext()
            .build()
        stub = VertxInferenceServiceGrpc.newVertxStub(channel)
    }

    fun chatCompletionStream(request: ChatRequest): Flow<ChatResponse> = callbackFlow {
         val stream = stub.chatCompletion(request)
         
         stream.handler { response ->
             trySend(response)
         }
         
         stream.exceptionHandler { t ->
             close(t)
         }
         
         stream.endHandler {
             close()
         }
         
         awaitClose {
             // specific logic to cancel stream if needed, usually grpc stream cancellation
         }
    }
}
