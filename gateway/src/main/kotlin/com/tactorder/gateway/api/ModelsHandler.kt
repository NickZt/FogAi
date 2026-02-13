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
                val nativeModels = models.map { 
                    it.toBuilder()
                        .setId("native-" + it.id)
                        .setObject("model (native)")
                        .build() 
                }
                
                // Convert to Map for JSON serialization (Jackson doesn't handle Proto well by default)
                val responseList = (models + nativeModels).map {
                    mapOf(
                        "id" to it.id,
                        "object" to it.`object`,
                        "created" to it.created,
                        "owned_by" to it.ownedBy
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
