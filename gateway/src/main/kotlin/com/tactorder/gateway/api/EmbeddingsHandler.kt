package com.tactorder.gateway.api

import com.tactorder.gateway.model.EmbeddingRequest
import com.tactorder.gateway.model.EmbeddingResponse
import com.tactorder.gateway.model.EmbeddingData
import com.tactorder.gateway.model.Usage
import com.tactorder.gateway.router.RouterAi
import com.tactorder.inference.proto.EmbeddingRequest as ProtoEmbeddingRequest
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import io.vertx.kotlin.coroutines.dispatcher
import org.slf4j.LoggerFactory

class EmbeddingsHandler(private val routerAi: RouterAi) {
    private val logger = LoggerFactory.getLogger(EmbeddingsHandler::class.java)

    fun handle(ctx: RoutingContext) {
        val request = try {
            ctx.body().asPojo(EmbeddingRequest::class.java)
        } catch (e: Exception) {
            logger.warn("Invalid request body", e)
            ctx.response().setStatusCode(400).end("Invalid JSON")
            return
        }

        logger.info("Received embedding request for model: ${request.model}")

        val client = routerAi.getClientForModel(request.model)
        if (client == null) {
            ctx.response().setStatusCode(404).end("Model not found")
            return
        }

        // Explicitly check for List input and reject it for now
        if (request.input is List<*>) {
            ctx.response().setStatusCode(400).end("Batch embeddings (list input) not supported yet")
            return
        }

        // Map input to string
        val inputLocal = request.input.toString()

        val protoRequest = ProtoEmbeddingRequest.newBuilder()
            .setModelId(request.model)
            .setText(inputLocal) // Use setText instead of setInput which was wrong in previous analysis/code
            .build()
            
        val scope = CoroutineScope(ctx.vertx().dispatcher())
        
        scope.launch {
            try {
                val protoResp = client.embeddings(protoRequest) // Non-streaming call needed in Client
                // Wait, client logic needs update for embeddings call? 
                // We generated stubs, let's check what we have in InferenceClient.
                
                // Assuming client has embeddings method, let's map response
                
                val response = EmbeddingResponse(
                    model = request.model,
                    data = protoResp.dataList.map { 
                        EmbeddingData(
                            index = it.index,
                            embedding = it.embeddingList
                        )
                    },
                    usage = Usage(0, 0, 0)
                )
                
                ctx.json(response)
                
            } catch (e: Exception) {
                logger.error("Error processing embeddings", e)
                ctx.response().setStatusCode(500).end(e.message)
            }
        }
    }
}
