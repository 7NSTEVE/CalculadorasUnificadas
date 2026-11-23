package com.calculadorasunificadas.app

import android.app.Activity
import android.content.Intent
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

class MainActivity : FlutterActivity() {
    companion object {
        private const val CHANNEL = "com.calculadorasunificadas.app/files"
        private const val REQ_PICK_FILE = 4101
        private const val REQ_SAVE_FILE = 4102
    }

    private var pendingResult: MethodChannel.Result? = null
    private var pendingSaveSource: String? = null
    private var pendingPickPrefix: String = "picked"

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "pickFile" -> {
                        if (pendingResult != null) {
                            result.error("BUSY", "Operacion de archivo en curso", null)
                            return@setMethodCallHandler
                        }
                        val mime = call.argument<String>("mimeType") ?: "*/*"
                        pendingPickPrefix = call.argument<String>("prefix") ?: "picked"
                        pendingResult = result
                        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                            addCategory(Intent.CATEGORY_OPENABLE)
                            type = mime
                        }
                        startActivityForResult(intent, REQ_PICK_FILE)
                    }

                    "saveFile" -> {
                        if (pendingResult != null) {
                            result.error("BUSY", "Operacion de archivo en curso", null)
                            return@setMethodCallHandler
                        }
                        val suggestedName = call.argument<String>("suggestedName") ?: "archivo.bin"
                        val mime = call.argument<String>("mimeType") ?: "application/octet-stream"
                        val sourcePath = call.argument<String>("sourcePath")
                        if (sourcePath.isNullOrEmpty() || !File(sourcePath).exists()) {
                            result.success(false)
                            return@setMethodCallHandler
                        }
                        pendingSaveSource = sourcePath
                        pendingResult = result
                        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                            addCategory(Intent.CATEGORY_OPENABLE)
                            type = mime
                            putExtra(Intent.EXTRA_TITLE, suggestedName)
                        }
                        startActivityForResult(intent, REQ_SAVE_FILE)
                    }

                    else -> result.notImplemented()
                }
            }
    }

    @Deprecated("Uses FlutterActivity onActivityResult for SAF document intents")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        val callback = pendingResult ?: return
        if (requestCode != REQ_PICK_FILE && requestCode != REQ_SAVE_FILE) {
            return
        }
        pendingResult = null

        val uri = data?.data
        if (resultCode != Activity.RESULT_OK || uri == null) {
            pendingSaveSource = null
            callback.success(if (requestCode == REQ_SAVE_FILE) false else null)
            return
        }

        if (requestCode == REQ_PICK_FILE) {
            try {
                val outFile = File(cacheDir, "${pendingPickPrefix}_${System.currentTimeMillis()}.bin")
                contentResolver.openInputStream(uri)?.use { input ->
                    FileOutputStream(outFile).use { output ->
                        input.copyTo(output)
                    }
                } ?: run {
                    callback.success(null)
                    return
                }
                callback.success(outFile.absolutePath)
            } catch (e: Exception) {
                callback.error("PICK_IO", e.message, null)
            }
        } else if (requestCode == REQ_SAVE_FILE) {
            val srcPath = pendingSaveSource
            pendingSaveSource = null
            if (srcPath.isNullOrEmpty()) {
                callback.success(false)
                return
            }
            try {
                FileInputStream(File(srcPath)).use { input ->
                    contentResolver.openOutputStream(uri)?.use { output ->
                        input.copyTo(output)
                    } ?: run {
                        callback.success(false)
                        return
                    }
                }
                callback.success(true)
            } catch (e: Exception) {
                callback.error("SAVE_IO", e.message, null)
            }
        }
    }
}
