package com.tactorder.gateway.api

import com.tactorder.gateway.domain.model.EmbeddingRequest
import io.vertx.core.Vertx
import io.vertx.core.json.JsonObject
import io.vertx.ext.web.RoutingContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import io.vertx.kotlin.coroutines.dispatcher
import org.slf4j.LoggerFactory
import io.vertx.kotlin.coroutines.coAwait
import io.vertx.core.eventbus.DeliveryOptions

class EmbeddingsHandler(private val vertx: Vertx) {
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
                val requestJson = JsonObject.mapFrom(request)
                val deliveryOptions = DeliveryOptions().setSendTimeout(300000) // 5 minutes
                val reply = vertx.eventBus().request<JsonObject>("mnn.embeddings.request", requestJson, deliveryOptions).coAwait()
                
                val data = reply.body()
                if (data.containsKey("error")) {
                    ctx.response().setStatusCode(500).end(data.getString("error"))
                } else if (data.containsKey("not_found")) {
                   ctx.response().setStatusCode(404).end("Model not found")
                } else if (data.containsKey("response")) {
                    ctx.json(data.getJsonObject("response"))
                } else {
                    ctx.response().setStatusCode(500).end("Invalid response from verticle")
                }
            } catch (e: Exception) {
                logger.error("Embedding generation failed over EventBus", e)
                if (!ctx.response().ended()) {
                    ctx.response().setStatusCode(500).end(e.message)
                }
            }
        }
    }
}
