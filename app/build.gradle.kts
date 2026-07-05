import com.android.build.api.dsl.ApplicationExtension

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.rust.android)
}

cargo {
    module = "./src/main/rust"
    libname = "qcxr"
    targets = listOf("arm64")
}

configure<ApplicationExtension> {
    namespace = "com.qcxr.questcraft"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.qcxr.questcraft"
        minSdk = 26
        //noinspection OldTargetApi
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters.addAll(listOf("arm64-v8a"))
            debugSymbolLevel = "FULL"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    buildFeatures {
        prefab = true
    }
}

dependencies {
    implementation(libs.openxr.loader)
    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.constraintlayout)
    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)
    androidTestImplementation(libs.espresso.core)
}

val rustJniLibsDir = layout.buildDirectory.dir("rustJniLibs/android").get()
tasks
    .matching { it.name.matches(Regex("merge.*JniLibFolders")) }
    .configureEach {
        inputs.dir(rustJniLibsDir)
        dependsOn("cargoBuild")
    }

tasks.matching { it.name.matches(Regex("merge.*Assets")) }.configureEach {
    inputs.files(compileSlangShaders.map { it.outputs.files })
}

val compileSlangShaders = tasks.register("compileSlangShaders") {
    group = "build"
    description = "Compiles .slang shaders to SPIR-V if slangc is available."

    val shaderDir = file("src/main/assets/shaders")

    inputs.dir(shaderDir)
    outputs.dir(shaderDir)

    doLast {
        val checkCommand = if (System.getProperty("os.name").contains("Windows")) listOf("where", "slangc") else listOf("which", "slangc")
        val isSlangcPresent = try {
            val process = ProcessBuilder(checkCommand).start()
            process.waitFor() == 0
        } catch (e: Exception) {
            false
        }

        if (isSlangcPresent) {
            println("Found slangc. Compiling shaders...")

            if (shaderDir.exists()) {
                shaderDir.walkTopDown().forEach { file ->
                    if (file.isFile && file.extension == "slang") {
                        val outputSpv = file.absolutePath.removeSuffix(".slang") + ".spv"
                        println("Compiling: ${file.name} -> ${file.name.removeSuffix(".slang")}.spv")

                        val compileProcess = ProcessBuilder("slangc", file.absolutePath, "-target", "spirv", "-o", outputSpv)
                            .inheritIO()
                            .start()

                        val exitCode = compileProcess.waitFor()
                        if (exitCode != 0) {
                            throw GradleException("slangc failed compiling ${file.name} with exit code $exitCode")
                        }
                    }
                }
            }
        } else {
            println("Notice: 'slangc' was not found in the system PATH. Skipping shader compilation.")
        }
    }
}