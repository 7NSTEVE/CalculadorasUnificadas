package com.calculadorasunificadas.app

import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import androidx.core.content.FileProvider
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
        private const val REQ_PICK_FILE_META = 4103
    }

    private var pendingResult: MethodChannel.Result? = null
    private var pendingSaveSource: String? = null
    private var pendingPickPrefix: String = "picked"

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "pickFile", "pickFileWithMeta" -> {
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
                        val reqCode = if (call.method == "pickFileWithMeta") {
                            REQ_PICK_FILE_META
                        } else {
                            REQ_PICK_FILE
                        }
                        startActivityForResult(intent, reqCode)
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

                    "copyImageToClipboard" -> {
                        val sourcePath = call.argument<String>("sourcePath")
                        val fileName = call.argument<String>("fileName") ?: "captura_cg50.png"
                        if (sourcePath.isNullOrEmpty() || !File(sourcePath).exists()) {
                            result.success(false)
                            return@setMethodCallHandler
                        }
                        try {
                            val shareDir = File(cacheDir, "clipboard_share")
                            if (!shareDir.exists()) shareDir.mkdirs()
                            val dest = File(shareDir, fileName)
                            File(sourcePath).copyTo(dest, overwrite = true)
                            val uri = FileProvider.getUriForFile(
                                this,
                                "${applicationContext.packageName}.fileprovider",
                                dest
                            )
                            val clipboard =
                                getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                            val clip = ClipData.newUri(contentResolver, "Captura", uri)
                            clipboard.setPrimaryClip(clip)
                            result.success(true)
                        } catch (e: Exception) {
                            result.success(false)
                        }
                    }

                    "openExternalUrl" -> {
                        val url = call.argument<String>("url") ?: ""
                        if (!url.startsWith("http://") && !url.startsWith("https://")) {
                            result.success(false)
                            return@setMethodCallHandler
                        }
                        try {
                            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url)).apply {
                                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                            }
                            startActivity(intent)
                            result.success(true)
                        } catch (e: Exception) {
                            result.success(false)
                        }
                    }

                    else -> result.notImplemented()
                }
            }
    }

    private fun resolveDisplayName(uri: Uri): String {
        try {
            contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (idx >= 0) {
                            val name = cursor.getString(idx)
                            if (!name.isNullOrBlank()) return name
                        }
                    }
                }
        } catch (_: Exception) {
        }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "${pendingPickPrefix}.bin"
    }

    @Deprecated("Uses FlutterActivity onActivityResult for SAF document intents")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        val callback = pendingResult ?: return
        if (requestCode != REQ_PICK_FILE &&
            requestCode != REQ_PICK_FILE_META &&
            requestCode != REQ_SAVE_FILE
        ) {
            return
        }
        pendingResult = null

        val uri = data?.data
        if (resultCode != Activity.RESULT_OK || uri == null) {
            pendingSaveSource = null
            callback.success(if (requestCode == REQ_SAVE_FILE) false else null)
            return
        }

        if (requestCode == REQ_PICK_FILE || requestCode == REQ_PICK_FILE_META) {
            try {
                val displayName = resolveDisplayName(uri)
                val safeName = displayName.replace(Regex("[^A-Za-z0-9._-]"), "_")
                val outFile = File(cacheDir, "${pendingPickPrefix}_${System.currentTimeMillis()}_$safeName")
                contentResolver.openInputStream(uri)?.use { input ->
                    FileOutputStream(outFile).use { output ->
                        input.copyTo(output)
                    }
                } ?: run {
                    callback.success(null)
                    return
                }
                if (requestCode == REQ_PICK_FILE_META) {
                    callback.success(mapOf("path" to outFile.absolutePath, "name" to displayName))
                } else {
                    callback.success(outFile.absolutePath)
                }
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
