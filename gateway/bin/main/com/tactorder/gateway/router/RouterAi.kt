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

    // Registry of nodes by prefix for fallback routing
    private val prefixRoutes = mutableMapOf<String, InferenceService>()
    
    // Fallback or default service
    private var defaultService: InferenceService? = null
    private val logger = org.slf4j.LoggerFactory.getLogger(RouterAi::class.java)

    lateinit var queueManager: com.tactorder.gateway.application.QueueManager
        private set

    init {
        // Load configuration
        logger.info("Initializing RouterAi...")
        loadConfig()
        logger.info("RouterAi initialized. Registered services: ${services.keys}")
    }

    private fun loadConfig() {
        logger.info("loading configuration...")
        
        // 1. Load .env for global settings (like models dir)
        val dotenv = io.github.cdimascio.dotenv.Dotenv.configure().ignoreIfMissing().load()
        val modelsDir = dotenv["MNN_MODELS_DIR"]
        
        // 2. Load nodes.json for service definition
        try {
            val configFile = java.io.File("nodes.json")
            if (configFile.exists()) {
                val json = io.vertx.core.json.JsonObject(configFile.readText())
                
                // Load Queue Config
                val queueObj = json.getJsonObject("queue")
                val queueConfig = if (queueObj != null) {
                    com.tactorder.gateway.application.QueueConfig(
                        criticalSlaMs = queueObj.getLong("criticalSlaMs", 1000L),
                        normalSlaMs = queueObj.getLong("normalSlaMs", 10000L),
                        backgroundSlaMs = queueObj.getLong("backgroundSlaMs", 300000L),
                        maxQueueSize = queueObj.getInteger("maxQueueSize", 1000)
                    )
                } else {
                    com.tactorder.gateway.application.QueueConfig()
                }
                queueManager = com.tactorder.gateway.application.QueueManager(queueConfig)
                
                val nodesArray = json.getJsonArray("nodes")
                
                nodesArray.forEach { node ->
                    if (node is io.vertx.core.json.JsonObject) {
                        val modelsArray = node.getJsonArray("models")
                        val explicitModels = mutableListOf<com.tactorder.gateway.domain.model.ModelConfig>()
                        if (modelsArray != null) {
                            modelsArray.forEach { modelRaw ->
                                if (modelRaw is io.vertx.core.json.JsonObject) {
                                    explicitModels.add(com.tactorder.gateway.domain.model.ModelConfig(
                                        id = modelRaw.getString("id"),
                                        path = modelRaw.getString("path")
                                    ))
                                }
                            }
                        }

                        val config = com.tactorder.gateway.domain.model.NodeConfig(
                            id = node.getString("id"),
                            type = node.getString("type"),
                            host = node.getString("host"),
                            port = node.getInteger("port"),
                            prefix = node.getString("prefix", ""),
                            models = explicitModels
                        )
                        
                        if (config.type == "mnn-jni") {
                            // SCAN local models if modelsDir is set
                            if (modelsDir != null) {
                                val dir = java.io.File(modelsDir)
                                if (dir.exists() && dir.isDirectory) {
                                    val scannedModels = mutableListOf<com.tactorder.gateway.domain.model.ModelConfig>()
                                    dir.listFiles()?.forEach { modelDir ->
                                        if (modelDir.isDirectory && java.io.File(modelDir, "config.json").exists()) {
                                            // Construct ID using the prefix from nodes.json
                                            val modelId = config.prefix + modelDir.name
                                            val path = java.io.File(modelDir, "config.json").absolutePath
                                            scannedModels.add(com.tactorder.gateway.domain.model.ModelConfig(modelId, path))
                                        }
                                    }
                                    val updatedConfig = config.copy(models = scannedModels)
                                    registerNode(updatedConfig)
                                } else {
                                     logger.warn("MNN_MODELS_DIR not found: $modelsDir")
                                }
                            }
                        } else {
                            // For gRPC or other types, just register the node (maybe with empty models list initially)
                            registerNode(config)
                        }
                    }
                }
            } else {
                logger.warn("nodes.json not found")
                queueManager = com.tactorder.gateway.application.QueueManager(com.tactorder.gateway.application.QueueConfig())
            }
        } catch (e: Exception) {
            logger.error("Failed to load nodes.json", e)
            if (!this::queueManager.isInitialized) {
                queueManager = com.tactorder.gateway.application.QueueManager(com.tactorder.gateway.application.QueueConfig())
            }
        }
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
                // Register models explicitly
                config.models.forEach { model ->
                    logger.info("Registering model: ${model.id}")
                    registerService(model.id, mnnService)
                }
                // Register prefix fallback
                if (config.prefix.isNotEmpty()) {
                    prefixRoutes[config.prefix] = mnnService
                }
            }
            "grpc" -> {
                if (config.host != null && config.port != null) {
                    val client = InferenceClient(vertx, config.host, config.port)
                    val service = GrpcInferenceService(client, config.prefix)
                     
                     logger.info("Registered gRPC client for node ${config.id} (prefix: ${config.prefix})")
                     if (config.prefix.isNotEmpty()) {
                        prefixRoutes[config.prefix] = service
                     }
                     config.models.forEach { model ->
                         logger.info("Registering specific gRPC model: ${model.id}")
                         registerService(model.id, service)
                     }
                }
            }
        }
    }
    
    fun getService(modelId: String): InferenceService? {
        // 1. Try exact match
        val exact = services[modelId]
        if (exact != null) return exact

        // 2. Try prefix match
        for ((prefix, service) in prefixRoutes) {
            if (modelId.startsWith(prefix)) {
                return service
            }
        }

        // 3. Fallback
        return defaultService ?: services.values.firstOrNull()
    }
    
    fun registerGrpcClient(modelId: String, host: String, port: Int) {
        val client = InferenceClient(vertx, host, port)
        val service = GrpcInferenceService(client, "")
        services[modelId] = service
        if (defaultService == null) defaultService = service
    }
    
    fun registerService(modelId: String, service: InferenceService) {
        services[modelId] = service
        if (defaultService == null) defaultService = service
    }

    suspend fun listModels(): List<String> {
        val allModels = mutableSetOf<String>()
        // 1. Local models
        allModels.addAll(services.keys)
        
        // 2. Remote models via prefixRoutes
        for ((prefix, service) in prefixRoutes) {
            if (service is GrpcInferenceService) {
                try {
                    val remoteIds = service.listModels()
                    // Prepend prefix to make them routable via prefixRoutes
                    remoteIds.forEach { id ->
                        allModels.add(prefix + id)
                    }
                } catch (e: Exception) {
                    logger.error("Failed to list models from remote service with prefix $prefix", e)
                }
            }
        }
        return allModels.toList()
    }
}
