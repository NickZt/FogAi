package com.tactorder.gateway

import com.tactorder.gateway.api.ChatCompletionHandler
import com.tactorder.gateway.router.RouterAi
import io.vertx.ext.web.Router
import io.vertx.ext.web.handler.BodyHandler
import io.vertx.kotlin.coroutines.CoroutineVerticle
import io.vertx.kotlin.coroutines.coAwait
import io.vertx.core.json.jackson.DatabindCodec
import org.slf4j.LoggerFactory


class GatewayVerticle : CoroutineVerticle() {
    private val logger = LoggerFactory.getLogger(GatewayVerticle::class.java)

    override suspend fun start() {
        // Register Kotlin module for Jackson
        DatabindCodec.mapper().registerModule(com.fasterxml.jackson.module.kotlin.KotlinModule.Builder().build())
        DatabindCodec.prettyMapper().registerModule(com.fasterxml.jackson.module.kotlin.KotlinModule.Builder().build())

        val router = Router.router(vertx)
        val gatewayRouterAi = RouterAi(vertx)
        
        // Body handler to parse JSON bodies
        router.route().handler(BodyHandler.create().setBodyLimit(50 * 1024 * 1024)) // 50MB limit

        // API Routes
        val modelsHandler = com.tactorder.gateway.api.ModelsHandler(gatewayRouterAi)
        router.get("/v1/models").handler(modelsHandler::handle)

        val chatService = com.tactorder.gateway.application.ChatService(gatewayRouterAi)
        val chatHandler = ChatCompletionHandler(chatService)
        router.post("/v1/chat/completions").handler(chatHandler::handle)
        
        val embeddingsHandler = com.tactorder.gateway.api.EmbeddingsHandler(gatewayRouterAi)
        router.post("/v1/embeddings").handler(embeddingsHandler::handle)

        
        // Health check
        router.get("/health").handler { ctx -> ctx.json(mapOf("status" to "ok")) }

        val port = config.getInteger("http.port", 8080)
        vertx.createHttpServer()
            .requestHandler(router)
            .listen(port)
            .coAwait()
            
        logger.info("HTTP Server listening on port $port")
    }
}
