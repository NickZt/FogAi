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
                val models = routerAi.listModels()
                ctx.json(mapOf(
                    "object" to "list",
                    "data" to models
                ))
            } catch (e: Exception) {
                e.printStackTrace()
                ctx.fail(e)
            }
        }
    }
}
