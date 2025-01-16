package tech.fastsense.common

import android.annotation.SuppressLint
import android.app.Service
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Binder
import android.os.FileObserver
import android.os.IBinder
import android.util.Log
import android.webkit.MimeTypeMap
import androidx.core.net.toUri
import androidx.preference.PreferenceManager
import com.google.firebase.storage.FirebaseStorage
import com.google.firebase.storage.StorageMetadata
import com.google.firebase.storage.StorageReference
import com.google.firebase.storage.UploadTask
import java.io.File
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread


class UploaderService : Service() {
    companion object {
        const val EXTERNAL_TRANSFER_ROOT =
            "/storage/emulated/0/Android/media/tech.fastsense.head/TransferToCloud"
        const val CREATE_DIR = 1073742080
        const val CLOUD_AUDIO_FOLDER = "audio_records"
        const val CLOUD_VIDEO_FOLDER = "video_records"
        const val CLOUD_LOG_FOLDER = "log_records"
        const val CLOUD_OTHER_FOLDER = "other"
        const val NEED_TO_PURGE_PREF = "need_to_purge"
        const val TAG = "UploaderService"
        fun log(s: String) = Log.e(TAG, s)
        val audioExts = setOf("mp3", "wav", "aac", "flac", "ogg", "m4a", "wma")
        val videoExts = setOf("mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v")
        val logExts = setOf("txt", "json", "jsonl", "log")
        private var instanceCallback: ((Boolean) -> Unit)? = null
        private var _uploadAllowed = AtomicBoolean(false)
        var uploadAllowed: Boolean
            get() = _uploadAllowed.get()
            set(value) {
                if (_uploadAllowed.get() == value) return
                log("uploadAllowed: $value")
                instanceCallback?.invoke(value)
                _uploadAllowed.set(value)
            }
    }
    //TODO simultaneous uploads limit

    private class DeferredUpload(
        val context: Context,
        val localUri: Uri,
        val cloudRef: StorageReference,
        val executor: Executor,
        val onSuccess: (DeferredUpload) -> Unit
    ) {
        private var task: UploadTask? = null

        fun getContentTypeFromUri(uri: Uri): String? {
            return when (uri.scheme) {
                ContentResolver.SCHEME_CONTENT -> {
                    // If it's a content URI, query the ContentResolver
                    context.contentResolver.getType(uri)
                }
                ContentResolver.SCHEME_FILE -> {
                    // If it's a file URI, determine the MIME type from the file extension
                    MimeTypeMap.getSingleton().getMimeTypeFromExtension(
                        MimeTypeMap.getFileExtensionFromUrl(uri.toString())
                    )
                }
                else -> null
            }
        }

        private fun init() {
            val m = StorageMetadata.Builder().setContentType(getContentTypeFromUri(localUri)).build()

            task = cloudRef.putFile(localUri, m).apply {
                addOnSuccessListener(executor) {
                    log("uploaded to storage: $cloudRef")
                    onSuccess(this@DeferredUpload)
                }
                addOnPausedListener(executor) {
                    log("upload paused ${100.0 * it.bytesTransferred / it.totalByteCount}: $cloudRef")
                }
                addOnProgressListener(executor) {
                    log("upload progress ${100.0 * it.bytesTransferred / it.totalByteCount}% : $cloudRef")
                }
            }
        }

        fun resume() = if (task == null) {
            init(); true
        } else task!!.resume()


        fun pause() = task?.pause() ?: false
    }

    private val executor = Executors.newFixedThreadPool(3)

    private val uploadTasks = ConcurrentLinkedQueue<DeferredUpload>()

    private val storage = FirebaseStorage.getInstance("gs://wehead-3362c-lmt8d/")

    private val deviceId
        get() = PreferenceManager.getDefaultSharedPreferences(this)
            .getString("device_id", "no_id")!!

    private val binder = LocalBinder()

    private val observers = mutableListOf<FileObserver>()

    private lateinit var thread: Thread

    private lateinit var root: File

    inner class LocalBinder : Binder() {
        fun getService() = this@UploaderService
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    override fun onDestroy() {
        super.onDestroy()
        thread.interrupt()
        observers.forEach { it.stopWatching() }
        executor.shutdown()
    }

    override fun onCreate() {
        super.onCreate()
        log("onCreate")
        root = File(EXTERNAL_TRANSFER_ROOT)
        if (!root.exists()) root.mkdir()
        instanceCallback = { state ->
            val action = if (state) DeferredUpload::resume else DeferredUpload::pause
            uploadTasks.forEach { action(it) }
        }
        thread = thread(name = "FileUploaderThread", block = ::task)

    }

    private val String.isAudio get() = lowercase() in audioExts

    private val File.allFiles get() = walkTopDown().filter { it.isFile }

    private val File.allSubdirs get() = walkTopDown().filter { it.isDirectory && it != this }

    private val String.cloudPath
        get() = try {
            // destructuring may throw
            val (date, chatAndFormat) = split('/').last().split("__")
            val (chat, format1) = chatAndFormat.split('.')
            val mediaTypeFolderName = when (format1.lowercase()) {
                in audioExts -> CLOUD_AUDIO_FOLDER
                in videoExts -> CLOUD_VIDEO_FOLDER
                in logExts -> CLOUD_LOG_FOLDER
                else -> throw RuntimeException("Unknown file extension")
            }
            "$mediaTypeFolderName/${deviceId.hId}/$chat/$date.$format1"
        } catch (e: Exception) {
            log("error on cloud path building (uploading to \"$CLOUD_OTHER_FOLDER\"): $e")
            "$CLOUD_OTHER_FOLDER/${deviceId.hId}/$this"
        }

    private val String.hId
        get() = try {
            "H" + split('-')[1]
        } catch (_: Exception) {
            this
        }

    private fun File.uploadRelativeTo(root: File) {
        if (!isFile) return
        val relPath = toRelativeString(root)
        log("file to upload: $this")
        val cloudPath = relPath.cloudPath
        val newTask = DeferredUpload(
            this@UploaderService,
            toUri(),
            storage.reference.child(cloudPath),
            executor,
        ) {
            uploadTasks.remove(it)
            val deleted = delete()
            log(if (deleted) "deleted: $relPath" else "error on file.delete(): $relPath")
        }

        uploadTasks.add(newTask)
        if (uploadAllowed || extension.isAudio) newTask.resume()

    }

    private fun File.observeRecursive() {
        if (!isDirectory) return
        // CREATE can be combined (by OS) with some internal bitflag to indicate what DIR is created,
        // so we need to react to combined CREATE_DIR event and add observer to it
        val observer = object : FileObserver(this, ALL_EVENTS) {
            override fun onEvent(event: Int, path: String?) {
                if (path != null) {
                    val entity = File(this@observeRecursive, path)
                    when (event) {
                        CREATE_DIR -> entity.observeRecursive()
                        CLOSE_WRITE, MOVED_TO -> entity.uploadRelativeTo(this@observeRecursive)
                    }
                }
            }
        }
        observers.add(observer)
        observer.startWatching()
        log("observer added for: $this")
    }

    @SuppressLint("ApplySharedPref")
    private fun checkedPurgeOldData() {
        val prefs = PreferenceManager.getDefaultSharedPreferences(this)
        if (!prefs.getBoolean(NEED_TO_PURGE_PREF, true)) return
        root.allFiles.forEach { it.delete() }
        root.allSubdirs.forEach { it.delete() }
        prefs.edit().putBoolean(NEED_TO_PURGE_PREF, false).commit()
    }

    private fun task() {
        log("thread started monitoring: $root")
        checkedPurgeOldData()
        // upload new files
        //TODO remove all empty subfolders to reduce dead observers memory footprint (BEFORE all uploads)
        root.allFiles.forEach { it.uploadRelativeTo(root) }
        // observe each dir recursively and dynamically
        root.observeRecursive()
        root.allSubdirs.forEach { it.observeRecursive() }
    }
}


