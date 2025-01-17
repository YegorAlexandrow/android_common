package tech.fastsense.common

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.util.Log
import java.net.HttpURLConnection
import java.net.URL
import kotlin.concurrent.thread

class CheckInternetService : Service() {
    private val binder = LocalBinder()

    @Volatile
    var isInternetAccessible = true

    private lateinit var requestThread: Thread
    private var running = true
    private val netAccRtr = 4
    private var netAccCtr = netAccRtr

    override fun onCreate() {
        super.onCreate()
        requestThread = thread(name = "requestThread", block = ::task)
    }

    inner class LocalBinder : Binder() {
        fun getService() = this@CheckInternetService
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    override fun onDestroy() {
        super.onDestroy()
        running = false
        requestThread.interrupt()
    }

    private fun task() {
        while (running) {
            val startTime = System.currentTimeMillis()
            val timeout = (2 + netAccRtr - netAccCtr) * 500

//            Log.d("CheckInternetService", "task() $netAccCtr $timeout")
            var urlc: HttpURLConnection? = null
            try {
                urlc = URL("http://www.google.com").openConnection() as HttpURLConnection
                //  urlc = URL("https://httpbin.org/delay/1").openConnection() as HttpURLConnection // DELAY TEST
                urlc.setRequestProperty("User-Agent", "Test")
                urlc.setRequestProperty("Connection", "close")
                urlc.connectTimeout = timeout
                urlc.readTimeout = timeout
                urlc.connect()
                if (urlc.responseCode == 200 || urlc.responseCode == 204) netAccCtr = netAccRtr
                else netAccCtr--
            } catch (e: Exception) {
                netAccCtr--
                Log.e("CheckInternetService", "Couldn't check internet connection: $e")
            } finally {
                urlc?.disconnect()
            }
            if (netAccCtr < 0) netAccCtr = 0
            isInternetAccessible = netAccCtr > 0
            val requestDuration = System.currentTimeMillis() - startTime
            val sleepTime = (timeout - requestDuration).coerceAtLeast(0L)
//            Log.d("CheckInternetService", "~task() $netAccCtr $sleepTime")

            try {
                Thread.sleep(sleepTime)
            } catch (e: InterruptedException) {
                Log.d("CheckInternetService", "Thread interrupted, stopping task.")
                break
            }
        }
    }
}

