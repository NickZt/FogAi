package com.tactorder.gateway.api

import io.vertx.ext.web.RoutingContext

class ModelsHandler {
    fun handle(ctx: RoutingContext) {
        val models = listOf(
            mapOf(
                "id" to "qwen2-0.5b-instruct",
                "object" to "model",
                "created" to System.currentTimeMillis() / 1000,
                "owned_by" to "tactorder"
            ),
             mapOf(
                "id" to "FogAI Chat",
                "object" to "model",
                "created" to System.currentTimeMillis() / 1000,
                "owned_by" to "tactorder"
            )
        )
        
        ctx.json(mapOf(
            "object" to "list",
            "data" to models
        ))
    }
}
