package com.tactorder.gateway

import io.vertx.core.Vertx
import io.vertx.kotlin.coroutines.CoroutineVerticle
import io.vertx.kotlin.coroutines.coAwait
import org.slf4j.LoggerFactory

suspend fun main() {
    val vertx = Vertx.vertx()
    try {
        vertx.deployVerticle(GatewayVerticle()).coAwait()
        LoggerFactory.getLogger("Main").info("Gateway started successfully")
    } catch (e: Exception) {
        LoggerFactory.getLogger("Main").error("Failed to start Gateway", e)
        vertx.close()
    }
}
