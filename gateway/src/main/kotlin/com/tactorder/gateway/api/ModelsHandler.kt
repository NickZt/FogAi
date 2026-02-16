package com.tactorder.gateway.api

import io.vertx.ext.web.RoutingContext
import com.tactorder.gateway.router.RouterAi
import kotlinx.coroutines.launch
import kotlinx.coroutines.CoroutineScope
import io.vertx.kotlin.coroutines.dispatcher

class ModelsHandler(private val routerAi: RouterAi) {
    fun handle(ctx: RoutingContext) {
        val scope = CoroutineScope(ctx.vertx().dispatcher())
        scope.launch {
            try {
                // Returns List<String> now
                val modelIds = routerAi.listModels()
                
                val responseList = modelIds.map { id ->
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
