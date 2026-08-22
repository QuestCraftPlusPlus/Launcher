import com.android.build.api.dsl.ApplicationExtension
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.rust.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.android)
}

cargo {
    module = "./src/main/rust"
    libname = "qcxr"
    targets = listOf("arm64")
//    features {
//        defaultAnd("profiled")
//    }
    profile = "release"
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

    sourceSets {
        getByName("main") {
            assets.directories += "src/generated/assets"
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
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    buildFeatures {
        prefab = true
    }
}

dependencies {
    api(files("libs/judgelib.aar"))
    // temp
    api(libs.msal4j)
    api(libs.gson)
    api(libs.slf4j.api)
    api(libs.okhttp3)

    implementation(libs.openxr.loader)
    implementation(platform(libs.compose.bom))
    implementation(libs.activity.compose)
    implementation(libs.core.ktx)
    implementation(libs.lifecycle.runtime.ktx)
    implementation(libs.material3)
    implementation(libs.compose.icons.extended)
    implementation(libs.ui)
    implementation(libs.ui.graphics)
    implementation(libs.ui.tooling.preview)
    implementation(libs.msal4j)
    implementation(libs.gson)
    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.constraintlayout)
/*    testImplementation(libs.junit)
    androidTestImplementation(platform(libs.compose.bom))
    androidTestImplementation(libs.ext.junit)
    androidTestImplementation(libs.espresso.core)
    androidTestImplementation(libs.ui.test.junit4)
    debugImplementation(libs.ui.test.manifest)
    debugImplementation(libs.ui.tooling)*/
}

val generatedShaderDir = layout.buildDirectory.dir("generated/shaders")

val compileSlangShaders = tasks.register<CompileSlangShadersTask>("compileSlangShaders") {
    group = "build"
    description = "Compiles .slang shaders to SPIR-V in src/generated/assets/shaders if slangc is available."

    inputDir.set(layout.projectDirectory.dir("src/main/assets/shaders"))
    outputDir.set(layout.projectDirectory.dir("src/generated/assets/shaders"))
}

tasks.matching {
    it.name.matches(Regex("merge.*Assets")) || it.name.contains("lint", ignoreCase = true)
}.configureEach {
    dependsOn(compileSlangShaders)
}

val rustJniLibsDir = layout.buildDirectory.dir("rustJniLibs/android").get()

tasks.matching { it.name.matches(Regex("merge.*JniLibFolders")) }.configureEach {
    inputs.dir(rustJniLibsDir)
    dependsOn("cargoBuild")
}

kotlin {
    compilerOptions {
        jvmTarget = JvmTarget.JVM_21
    }
}

gradle.taskGraph.whenReady {
    allTasks.forEach { task ->
        if (task.name.contains("lint", ignoreCase = true)) {
            task.enabled = false
        }
    }
}

abstract class CompileSlangShadersTask : DefaultTask() {

    @get:InputDirectory
    abstract val inputDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun compile() {
        val inFolder = inputDir.get().asFile
        val outFolder = outputDir.get().asFile

        val checkCommand = if (System.getProperty("os.name").contains("Windows")) {
            listOf("where", "slangc")
        } else {
            listOf("which", "slangc")
        }

        val isSlangcPresent = try {
            ProcessBuilder(checkCommand).start().waitFor() == 0
        } catch (e: Exception) {
            false
        }

        if (isSlangcPresent && inFolder.exists()) {
            inFolder.walkTopDown().forEach { file ->
                if (file.isFile && file.extension == "slang") {
                    val relativePath = file.relativeTo(inFolder).path.removeSuffix(".slang") + ".spv"
                    val outputFile = File(outFolder, relativePath)

                    outputFile.parentFile?.mkdirs()

                    val compileProcess = ProcessBuilder(
                        "slangc", file.absolutePath, "-target", "spirv", "-o", outputFile.absolutePath
                    ).redirectErrorStream(true).start()

                    val output = compileProcess.inputStream.bufferedReader().readText()
                    if (output.isNotBlank()) {
                        logger.lifecycle(output.trim())
                    }

                    val exitCode = compileProcess.waitFor()
                    if (exitCode != 0) {
                        throw GradleException("slangc failed compiling ${file.name} with exit code $exitCode, see log for details")
                    }
                }
            }
        } else if (!isSlangcPresent) {
            println("Notice: 'slangc' was not found in PATH. Using pre-compiled .spv files in src/generated/assets/shaders if present.")
        }
    }
}