package com.tactorder.gateway.api

import io.vertx.ext.web.RoutingContext
import io.vertx.core.Vertx
import io.vertx.core.json.JsonArray
import kotlinx.coroutines.launch
import kotlinx.coroutines.CoroutineScope
import io.vertx.kotlin.coroutines.dispatcher
import io.vertx.kotlin.coroutines.coAwait

class ModelsHandler(private val vertx: Vertx) {
    fun handle(ctx: RoutingContext) {
        val scope = CoroutineScope(ctx.vertx().dispatcher())
        scope.launch {
            try {
                // Request from ChatVerticle
                val chatModelsReply = try {
                    vertx.eventBus().request<JsonArray>("mnn.models.chat.request", "").coAwait().body().map { it.toString() }
                } catch (e: Exception) { emptyList() }
                
                // Request from EmbeddingsVerticle
                val embedModelsReply = try {
                    vertx.eventBus().request<JsonArray>("mnn.models.embeddings.request", "").coAwait().body().map { it.toString() }
                } catch (e: Exception) { emptyList() }

                val allModels = (chatModelsReply + embedModelsReply).distinct()
                
                val responseList = allModels.map { id ->
                    mapOf(
                        "id" to id,
                        "object" to "model",
                        "created" to System.currentTimeMillis() / 1000,
                        "owned_by" to "mnn-gateway"
                    )
                }

                ctx.json(mapOf(
                    "object" to "list",
                    "data" to responseList
                ))
            } catch (e: Exception) {
                e.printStackTrace()
                ctx.fail(e)
            }
        }
    }
}
