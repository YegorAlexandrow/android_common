package tech.fastsense.common.app_update

import android.annotation.SuppressLint
import android.app.DownloadManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import androidx.core.content.FileProvider
import androidx.preference.PreferenceManager
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import tech.fastsense.common.StatusLogger
import tech.fastsense.common.local_logger.LocalLogger
import java.io.File
import java.util.Timer
import java.util.TimerTask

class InstallManager(private val context: Context) : KoinComponent {
    private val localLogger: LocalLogger by inject()
    private var currentDownloadId: Long? = null
    private var downloadManager: DownloadManager =
        context.getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager
    private var downloadStatusTimer: Timer = Timer()
    private var downloadStatusListener: ((String, Int, Int, Int) -> Unit)? = null
    private val statusLogger: StatusLogger by inject()
    private var onCompleteDownloadReceiver: BroadcastReceiver? = null

    companion object {
        // TODO unhardcode package name (not required now)
        private const val APK_DOWNLOAD_DIR =
            "/storage/emulated/0/Android/media/tech.fastsense.head/Download"
    }

    fun downloadAndInstall(
        url: String,
        appId: String,
        versionName: String,
        callback: ((String, Int, Int, Int) -> Unit)?
    ) {
        val dest = "$APK_DOWNLOAD_DIR/app-$versionName.apk"

        cancelDownload()

        startApkDownload(url, dest)

        showInstallOption(dest, appId, versionName)

        if (callback != null) registerDownloadStatusListener(versionName, callback)
    }

    fun cancelDownload() {
        currentDownloadId?.let { downloadManager.remove(it) }

        try {
            if (onCompleteDownloadReceiver != null) context.unregisterReceiver(
                onCompleteDownloadReceiver
            )
        } catch (e: Exception) {}

        try {
            downloadStatusTimer.cancel()
            downloadStatusTimer.purge()
        } catch (e: Exception) {}
    }

    private fun registerDownloadStatusListener(
        versionName: String,
        callback: (String, Int, Int, Int) -> Unit
    ) {
        try {
            downloadStatusTimer.cancel()
            downloadStatusTimer.purge()
        } catch (e: Exception) {
        }

        downloadStatusListener = callback

        downloadStatusTimer = Timer()

        downloadStatusTimer.schedule(object : TimerTask() {
            @SuppressLint("Range")
            override fun run() {
                if (currentDownloadId == null) return

                val query = DownloadManager.Query().setFilterById(currentDownloadId!!)
                val c = downloadManager.query(query)

                c.moveToFirst()

                val bytesDownloaded =
                    c.getInt(c.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR))
                val bytesTotal =
                    c.getInt(c.getColumnIndex(DownloadManager.COLUMN_TOTAL_SIZE_BYTES))
                val status: Int =
                    c.getInt(c.getColumnIndex(DownloadManager.COLUMN_STATUS))

                callback(versionName, status, bytesDownloaded, bytesTotal)

                c.close()
            }

        }, 0, 500)
    }

    private fun deleteRecursive(fileOrDirectory: File) {
        if (fileOrDirectory.isDirectory) {
            for (child in fileOrDirectory.listFiles()!!) {
                deleteRecursive(child)
            }
        }
        fileOrDirectory.delete()
    }

    private fun startApkDownload(url: String, destination: String) {
        val destUri = Uri.parse("file://$destination")

        val downloadDir = File(APK_DOWNLOAD_DIR)
        if (downloadDir.exists()) deleteRecursive(downloadDir)

        val downloadUri = Uri.parse(url)
        val request = DownloadManager.Request(downloadUri)
        request.setMimeType("application/vnd.android.package-archive")
        request.setTitle("Downloading new version of head app")
        request.setDescription("Downloading new version of head app")
        request.setDestinationUri(destUri)

        currentDownloadId = downloadManager.enqueue(request)
    }

    @SuppressLint("Range")
    private fun isCurrentDownloadSuccess(): Boolean {
        if (currentDownloadId == null) return false

        val query = DownloadManager.Query().setFilterById(currentDownloadId!!)
        val cursor = downloadManager.query(query)

        if (cursor.moveToFirst()) {
            val status: Int = cursor.getInt(cursor.getColumnIndex(DownloadManager.COLUMN_STATUS))
            return status == DownloadManager.STATUS_SUCCESSFUL
        }

        return false
    }

    private fun installApkImpl(destination: String, appId: String, versionName: String) {
        val mSettings = PreferenceManager.getDefaultSharedPreferences(context)
        mSettings.edit().putString("apk_to_install", versionName).apply()
        mSettings.edit().putLong("apk_to_install_ts", System.currentTimeMillis()).apply()

        val contentUri = FileProvider.getUriForFile(
            context,
            "$appId.provider",
            File(destination)
        )
        val install = Intent(Intent.ACTION_VIEW)
        install.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        install.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        install.putExtra(Intent.EXTRA_NOT_UNKNOWN_SOURCE, true)
        install.data = contentUri
        localLogger("Updating Head App", LocalLogger.LogLevel.SPECIAL, async = false)
            .invokeOnCompletion { context.startActivity(install) }

        statusLogger.updateStatus("off", "install_apk")
    }

    fun installApk(appId: String, versionName: String) {
        val dest = "$APK_DOWNLOAD_DIR/app-$versionName.apk"
        installApkImpl(dest, appId, versionName)
    }

    private fun showInstallOption(destination: String, appId: String, versionName: String) {
        onCompleteDownloadReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                if (isCurrentDownloadSuccess()) {
                    installApkImpl(destination, appId, versionName)
                }
                context.unregisterReceiver(this)

                downloadStatusListener?.let { it("", DownloadManager.STATUS_SUCCESSFUL, 100, 100) }

                currentDownloadId = null
                onCompleteDownloadReceiver = null
                downloadStatusListener = null
            }
        }
        context.registerReceiver(
            onCompleteDownloadReceiver,
            IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE)
        )
    }
}