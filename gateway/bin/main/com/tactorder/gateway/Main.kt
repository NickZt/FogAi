package com.tactorder.gateway

import com.tactorder.gateway.verticles.ChatVerticle
import com.tactorder.gateway.verticles.EmbeddingsVerticle
import io.vertx.core.Vertx
import io.vertx.kotlin.coroutines.CoroutineVerticle
import io.vertx.kotlin.coroutines.coAwait
import org.slf4j.LoggerFactory

suspend fun main() {
    val vertx = Vertx.vertx()
    try {
        vertx.deployVerticle(ChatVerticle()).coAwait()
        vertx.deployVerticle(EmbeddingsVerticle()).coAwait()
        vertx.deployVerticle(GatewayVerticle()).coAwait()
        LoggerFactory.getLogger("Main").info("Gateway started successfully")
    } catch (e: Exception) {
        LoggerFactory.getLogger("Main").error("Failed to start Gateway", e)
        vertx.close()
    }
}
