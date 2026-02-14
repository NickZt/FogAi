package com.tactorder.gateway.domain.model

data class NodeConfig(
    val id: String,
    val type: String, // "mnn-jni", "grpc"
    val host: String? = null,
    val port: Int? = null,
    val models: List<ModelConfig> = emptyList()
)

data class ModelConfig(
    val id: String,
    val path: String? = null // For local MNN models
)
