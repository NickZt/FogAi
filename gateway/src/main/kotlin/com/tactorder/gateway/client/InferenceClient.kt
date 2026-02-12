package com.tactorder.gateway.client

import com.tactorder.inference.proto.ChatRequest
import com.tactorder.inference.proto.ChatResponse
import com.tactorder.inference.proto.InferenceServiceGrpc
import io.vertx.core.Vertx
import io.vertx.core.net.SocketAddress
import io.vertx.grpc.client.GrpcClient
import io.vertx.grpc.client.GrpcClientChannel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import io.vertx.kotlin.coroutines.coAwait

class InferenceClient(private val vertx: Vertx, private val host: String, private val port: Int) {
    
    // We utilize the new Vert.x gRPC Client (which might differ significantly from the stub-based approach)
    // However, since we generated stubs using `vertx-grpc`, we can use `VertxInferenceServiceGrpc`.
    // Let's use the stub approach for simplicity if compatible.
    
    // Actually, `vertx-grpc` (stubs) uses `ManagedChannel` from `grpc-netty` wrapped with Vert.x context.
    // But `vertx-grpc-client` is the new way.
    // Given we used `vertx-grpc` dependency and the plugin, we have `VertxInferenceServiceGrpc`.
    
    private val stub: InferenceServiceGrpc.InferenceServiceVertxStub

    init {
        val channel = io.vertx.grpc.VertxChannelBuilder
            .forAddress(vertx, host, port)
            .usePlaintext()
            .build()
        stub = InferenceServiceGrpc.newVertxStub(channel)
    }

    fun chatCompletionStream(request: ChatRequest): Flow<ChatResponse> = flow {
        val responseChannel = io.vertx.grpc.VertxServerCalls.OneToOneStreamObserver<ChatRequest, ChatResponse>()
         // The stub method signature for streaming response in Vert.x gRPC usually takes a handler or similar.
         // Let's check generated code... usually it returns a ReadStream or similar in strict Vert.x mode, 
         // OR it uses StreamObserver.
         
         // With standard gRPC stubs + Vert.x Channel:
         // stub.chatCompletion(request, observer)
         
         // Let's implement using callback-to-flow conversion.
         val queue = kotlinx.coroutines.channels.Channel<ChatResponse>(kotlinx.coroutines.channels.Channel.UNLIMITED)
         
         stub.chatCompletion(request, object : io.grpc.stub.StreamObserver<ChatResponse> {
             override fun onNext(value: ChatResponse) {
                 queue.trySend(value)
             }
             override fun onError(t: Throwable) {
                 queue.close(t)
             }
             override fun onCompleted() {
                 queue.close()
             }
         })
         
         for (msg in queue) {
             emit(msg)
         }
    }
}
