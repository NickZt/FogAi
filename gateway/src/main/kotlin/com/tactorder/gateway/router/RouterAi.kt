package com.tactorder.gateway.router

import com.tactorder.gateway.client.InferenceClient
import com.tactorder.gateway.domain.port.InferenceService
import com.tactorder.gateway.infrastructure.grpc.GrpcInferenceService
import com.tactorder.gateway.infrastructure.mnn.MnnJniService
import com.tactorder.gateway.infrastructure.mnn.NativeEngineWrapper
import io.vertx.core.Vertx

class RouterAi(private val vertx: Vertx) {
    // Registry of services by model ID
    private val services = mutableMapOf<String, InferenceService>()
    
    // Fallback or default service
    private var defaultService: InferenceService? = null
    private val logger = org.slf4j.LoggerFactory.getLogger(RouterAi::class.java)

    init {
        // Load configuration
        logger.info("Initializing RouterAi...")
        loadConfig()
        logger.info("RouterAi initialized. Registered services: ${services.keys}")
    }

    private fun loadConfig() {
        logger.info("Loading default configuration...")
        // Simple JSON config loading (could use Jackson or Vert.x Config)
        // For MVP, manual setup based on env or default file
        
        // Define Default Local Node
        val localNode = com.tactorder.gateway.domain.model.NodeConfig(
            id = "local-mnn",
            type = "mnn-jni",
            models = listOf(
                com.tactorder.gateway.domain.model.ModelConfig("native-qwen2-0.5b-instruct", "/home/nickzt/Projects/LLM_campf/models_mnn/qwen2-0.5b-instruct/config.json"),
                com.tactorder.gateway.domain.model.ModelConfig("native-embedding-0.6b", "/home/nickzt/Projects/LLM_campf/models_mnn/Qwen3-Embedding-0.6B-MNN/config.json")
            )
        )
        registerNode(localNode)
    }

    fun registerNode(config: com.tactorder.gateway.domain.model.NodeConfig) {
        logger.info("Registering node: ${config.id} type=${config.type}")
        when (config.type) {
            "mnn-jni" -> {
                val mnnService = MnnJniService(
                    nativeEngine = NativeEngineWrapper(),
                    modelPathResolver = { modelId ->
                        config.models.find { it.id == modelId }?.path 
                            ?: throw IllegalArgumentException("Model path not configured for $modelId in node ${config.id}")
                    }
                )
                config.models.forEach { model ->
                    logger.info("Registering model: ${model.id}")
                    registerService(model.id, mnnService)
                }
            }
// ...
            "grpc" -> {
                if (config.host != null && config.port != null) {
                    val client = InferenceClient(vertx, config.host, config.port)
                    val service = GrpcInferenceService(client)
                    config.models.forEach { model ->
                        registerService(model.id, service)
                    }
                }
            }
        }
    }
    
    fun getService(modelId: String): InferenceService? {
        if (modelId.startsWith("native-")) {
             // For now, if it starts with native, return the mnn service even if not explicitly registered by ID
             // This allows dynamic native model loading if we had a better path resolver
             return services.values.filterIsInstance<MnnJniService>().firstOrNull() 
        }
        return services[modelId] ?: defaultService ?: services.values.firstOrNull()
    }
    
    fun registerGrpcClient(modelId: String, host: String, port: Int) {
        val client = InferenceClient(vertx, host, port)
        val service = GrpcInferenceService(client)
        services[modelId] = service
        if (defaultService == null) defaultService = service
    }
    
    fun registerService(modelId: String, service: InferenceService) {
        services[modelId] = service
        if (defaultService == null) defaultService = service
    }

    suspend fun listModels(): List<String> {
        // Return keys of registered services
        return services.keys.toList()
    }
}
