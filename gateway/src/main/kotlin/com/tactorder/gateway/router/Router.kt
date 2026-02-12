package com.tactorder.gateway.router

import com.tactorder.gateway.client.InferenceClient
import io.vertx.core.Vertx

class Router(private val vertx: Vertx) {
    // fast look-up map for clients
    private val clients = mutableMapOf<String, InferenceClient>()
    
    init {
        // MOCK REGISTRATION
        // In real world, this would verify availability
        clients["mnn-llama3"] = InferenceClient(vertx, "localhost", 50051)
    }
    
    fun getClientForModel(modelId: String): InferenceClient? {
        // Simple logic: direct match or default
        return clients[modelId] ?: clients.values.firstOrNull()
    }
    
    fun registerClient(modelId: String, host: String, port: Int) {
        clients[modelId] = InferenceClient(vertx, host, port)
    }
}
