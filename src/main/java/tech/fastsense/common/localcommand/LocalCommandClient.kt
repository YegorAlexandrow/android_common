package tech.fastsense.common.localcommand

import android.util.Log
import com.google.gson.Gson
import io.ktor.client.HttpClient
import io.ktor.client.engine.okhttp.OkHttp
import io.ktor.client.plugins.websocket.DefaultClientWebSocketSession
import io.ktor.client.plugins.websocket.WebSockets
import io.ktor.client.plugins.websocket.webSocket
import io.ktor.http.HttpMethod
import io.ktor.websocket.Frame
import io.ktor.websocket.close
import io.ktor.websocket.readText
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch

class LocalCommandClient(private val port: Int = LocalCommandServer.DEFAULT_PORT) {
    fun interface CommandListener {
        operator fun invoke(command: LocalCommand)
    }

    fun interface ConnectionListener {
        operator fun invoke(connected: Boolean)
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val commands = Channel<LocalCommand>(Channel.UNLIMITED)
    private val gson = Gson()

    private var client: HttpClient? = null
    private var host: String? = null
    private var running = false
    private var connectionJob: Job? = null

    var onReceivedCommand: CommandListener? = null
    var onConnectionChanged: ConnectionListener? = null

    val isRunning: Boolean
        get() = running

    fun start(host: String) {
        stop()
        this.host = host
        running = true
        client = HttpClient(OkHttp) {
            install(WebSockets)
        }
        connectionJob = scope.launch {
            while (isActive && running) {
                try {
                    client?.webSocket(
                        method = HttpMethod.Get,
                        host = host,
                        port = port,
                        path = "/",
                    ) {
                        onConnectionChanged?.invoke(true)
                        log("connected to $host:$port")
                        try {
                            listOf(
                                async(Dispatchers.IO) { outputLoop() },
                                async(Dispatchers.IO) { inputLoop() },
                                async(Dispatchers.IO) { pingLoop() },
                            ).joinAll()
                        } finally {
                            onConnectionChanged?.invoke(false)
                            runCatching { close() }
                        }
                    }
                } catch (e: Exception) {
                    onConnectionChanged?.invoke(false)
                    if (running) {
                        loge("connection failed to $host:$port: $e")
                    }
                    delay(1_000L)
                }
            }
        }
    }

    fun stop() {
        running = false
        connectionJob?.cancel()
        connectionJob = null
        onConnectionChanged?.invoke(false)
        runCatching { client?.close() }
        client = null
    }

    suspend fun send(command: LocalCommand) {
        commands.send(command)
    }

    private suspend fun DefaultClientWebSocketSession.inputLoop() {
        for (frame in incoming) {
            frame as? Frame.Text ?: continue
            val text = frame.readText()
            if (text == PING) continue
            try {
                val command = gson.fromJson(text, LocalCommand::class.java)
                log("received: $command")
                onReceivedCommand?.invoke(command)
            } catch (e: Exception) {
                loge("failed to parse command: $text")
            }
        }
    }

    private suspend fun DefaultClientWebSocketSession.outputLoop() {
        for (command in commands) {
            send(Frame.Text(gson.toJson(command)))
        }
    }

    private suspend fun DefaultClientWebSocketSession.pingLoop() {
        while (true) {
            val result = this.outgoing.trySend(Frame.Text(PING))
            if (result.isFailure || result.isClosed) {
                throw IllegalStateException("Ping failed")
            }
            delay(1_000L)
        }
    }

    companion object {
        private const val TAG = "LocalCommandClient"
        private const val PING = "ping"

        private fun log(message: String) = Log.i(TAG, message)
        private fun loge(message: String) = Log.e(TAG, message)
    }
}
