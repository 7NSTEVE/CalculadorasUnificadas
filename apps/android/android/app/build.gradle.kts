import java.io.File
import java.io.FileInputStream
import java.security.MessageDigest
import java.util.Properties

plugins {
    id("com.android.application")
    id("dev.flutter.flutter-gradle-plugin")
}

val keystoreProperties = Properties()
val keystorePropertiesFile = rootProject.file("key.properties")
if (keystorePropertiesFile.exists()) {
    FileInputStream(keystorePropertiesFile).use { keystoreProperties.load(it) }
}

fun sha256Of(file: File): String {
    val md = MessageDigest.getInstance("SHA-256")
    file.inputStream().use { input ->
        val buf = ByteArray(65536)
        while (true) {
            val r = input.read(buf)
            if (r <= 0) break
            md.update(buf, 0, r)
        }
    }
    return md.digest().joinToString("") { "%02x".format(it) }
}

val syncSharedAssets by tasks.registering {
    doLast {
        val monoRoot = rootProject.projectDir.parentFile.parentFile.parentFile
        val srcAssets = File(monoRoot, "assets")
        val dstStaged = File(rootProject.projectDir.parentFile, "assets_staged")
        val pairs = listOf(
            "skins" to listOf(
                "caratula_cg50.png",
                "caratula_cw2.png",
                "caratula_classwiz.png",
                "caratula_esplus.png"
            ),
            "roms" to listOf(
                "SystemROM.raw",
                "UserROM.raw",
                "C2ROM.raw",
                "official-ILRAM-FD800000-4000.bin",
                "cg50_warm_state.bin.gz",
                "fx991cw_rom.bin",
                "fx991cw_default_en.ram",
                "fx991lax.rom",
                "fx570vnplus.rom"
            )
        )
        for ((sub, names) in pairs) {
            val outDir = File(dstStaged, sub)
            outDir.mkdirs()
            for (name in names) {
                val src = File(File(srcAssets, sub), name)
                if (!src.exists()) continue
                val dst = File(outDir, name)
                if (dst.exists() && dst.length() == src.length() && sha256Of(dst) == sha256Of(src)) {
                    continue
                }
                src.copyTo(dst, overwrite = true)
            }
        }
    }
}

android {
    namespace = "com.calculadorasunificadas.app"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = "28.2.13676358"

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        applicationId = "com.calculadorasunificadas.app"
        minSdk = 24
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-O3", "-fexceptions")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../../../core/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }

    signingConfigs {
        if (keystorePropertiesFile.exists()) {
            create("release") {
                keyAlias = keystoreProperties.getProperty("keyAlias")
                keyPassword = keystoreProperties.getProperty("keyPassword")
                storeFile = file(keystoreProperties.getProperty("storeFile"))
                storePassword = keystoreProperties.getProperty("storePassword")
            }
        }
    }

    buildTypes {
        release {
            signingConfig = if (keystorePropertiesFile.exists()) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }
}

tasks.matching { it.name == "preBuild" }.configureEach {
    dependsOn(syncSharedAssets)
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}
