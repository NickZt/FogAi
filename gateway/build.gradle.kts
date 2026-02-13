plugins {
    kotlin("jvm") version "1.9.22"
    application
    id("com.google.protobuf") version "0.9.4"
}

repositories {
    mavenCentral()
}

val vertxVersion = "4.5.3"
val grpcVersion = "1.61.1"
val protobufVersion = "3.25.3"
val coroutinesVersion = "1.8.0"

dependencies {
    implementation(platform("io.vertx:vertx-stack-depchain:$vertxVersion"))
    implementation("io.vertx:vertx-core")
    implementation("io.vertx:vertx-web")
    implementation("io.vertx:vertx-grpc") // Required for generated gRPC stubs
    implementation("io.vertx:vertx-grpc-client")
    
    // Kotlin
    implementation("io.vertx:vertx-lang-kotlin")
    implementation("io.vertx:vertx-lang-kotlin-coroutines")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:$coroutinesVersion")
    
    // Jackson
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin:2.15.3")
    implementation("com.fasterxml.jackson.core:jackson-annotations:2.15.3")

    // gRPC & Protobuf
    implementation("io.grpc:grpc-netty:$grpcVersion")
    implementation("io.grpc:grpc-protobuf:$grpcVersion")
    implementation("io.grpc:grpc-stub:$grpcVersion")
    implementation("com.google.protobuf:protobuf-java-util:$protobufVersion") // For JSON conversion if needed
    implementation("javax.annotation:javax.annotation-api:1.3.2") // Fix for missing Generated annotation
    
    // Logging
    implementation("org.slf4j:slf4j-api:2.0.12")
    implementation("ch.qos.logback:logback-classic:1.5.3")
}

application {
    mainClass.set("com.tactorder.gateway.MainKt")
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile> {
    kotlinOptions.jvmTarget = "17"
}

sourceSets {
    main {
        proto {
            srcDir("../proto") // Point to the shared proto directory
        }
    }
}

tasks.withType<JavaExec> {
    systemProperty("java.library.path", file("libs").absolutePath)
}

val cmakeDir = file("../inference-services/mnn-service")
val buildDir = file("../inference-services/mnn-service/build")
val libDir = file("libs")
val libName = "libmnn_bridge.so"

tasks.register<Exec>("cmakeConfig") {
    workingDir(cmakeDir)
    commandLine("mkdir", "-p", "build")
}

tasks.register<Exec>("compileJni") {
    dependsOn("cmakeConfig")
    workingDir(buildDir)
    // Run cmake and make. Re-running make is fast if nothing changed.
    commandLine("sh", "-c", "cmake .. -DMNN_BUILD_LLM=ON -DMNN_AVX512=ON -DMNN_IMGCODECS=ON && make -j8")
}

tasks.register<Copy>("copyJniLib") {
    dependsOn("compileJni")
    from(buildDir.resolve(libName))
    into(libDir)
}

// Ensure the library is built and copied before processing resources or running
tasks.named("processResources") {
    dependsOn("copyJniLib")
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:$protobufVersion"
    }
    plugins {
        create("grpc") {
            artifact = "io.grpc:protoc-gen-grpc-java:$grpcVersion"
        }
        create("vertx") {
            artifact = "io.vertx:vertx-grpc-protoc-plugin:$vertxVersion"
        }
    }
    generateProtoTasks {
        ofSourceSet("main").forEach {
            it.plugins {
                create("grpc")
                create("vertx")
            }
        }
    }
}
