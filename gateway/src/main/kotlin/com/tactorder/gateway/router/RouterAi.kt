package com.tactorder.gateway.router

import com.tactorder.gateway.client.InferenceClient
import io.vertx.core.Vertx

class RouterAi(private val vertx: Vertx) {
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

    suspend fun listModels(): List<Map<String, Any>> {
        val allModels = mutableListOf<Map<String, Any>>()
        
        // Scan all registered clients
        // Deduplicate by ID if needed, or prefix with client name?
        // For now, assuming single main client or merging lists
        for (client in clients.values.distinct()) {
            try {
                val response = client.listModels()
                response.modelsList.forEach { model ->
                    allModels.add(mapOf(
                        "id" to model.id,
                        "object" to "model",
                        "created" to model.created,
                        "owned_by" to model.ownedBy
                    ))
                }
            } catch (e: Exception) {
                // Ignore failed clients
                e.printStackTrace()
            }
        }
        
        // Ensure at least Fallback/Mock models if empty (or better, valid models only)
        if (allModels.isEmpty()) {
             // Fallback for demo if service is down
             /*
             allModels.add(mapOf(
                "id" to "qwen2-0.5b-instruct-fallback",
                "object" to "model",
                "created" to System.currentTimeMillis() / 1000,
                "owned_by" to "tactorder"
             ))
             */
        }
        
        return allModels
    }
}
