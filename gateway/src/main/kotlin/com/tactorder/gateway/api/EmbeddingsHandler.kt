package com.tactorder.gateway.api

import com.tactorder.gateway.domain.model.EmbeddingRequest
import com.tactorder.gateway.router.RouterAi
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import io.vertx.kotlin.coroutines.dispatcher
import org.slf4j.LoggerFactory

class EmbeddingsHandler(private val routerAi: RouterAi) {
    private val logger = LoggerFactory.getLogger(EmbeddingsHandler::class.java)

    fun handle(ctx: RoutingContext) {
        val body = ctx.body().asJsonObject()
        
        val request = try {
            val inputObj = body.getValue("input")
            val inputList = when (inputObj) {
                is String -> listOf(inputObj)
                is io.vertx.core.json.JsonArray -> inputObj.list.map { it.toString() }
                is List<*> -> inputObj.map { it.toString() }
                else -> emptyList()
            }
            
            EmbeddingRequest(
                model = body.getString("model"),
                input = inputList
            )
        } catch (e: Exception) {
            logger.warn("Invalid request body", e)
            ctx.response().setStatusCode(400).end("Invalid JSON")
            return
        }

        val scope = CoroutineScope(ctx.vertx().dispatcher())
        scope.launch {
            try {
                val service = routerAi.getService(request.model)
                if (service == null) {
                    ctx.response().setStatusCode(404).end("Model not found")
                    return@launch
                }
                
                val response = service.embeddings(request)
                ctx.json(response)
                
            } catch (e: Exception) {
                logger.error("Embedding generation failed", e)
                ctx.fail(500, e)
            }
        }
    }
}
