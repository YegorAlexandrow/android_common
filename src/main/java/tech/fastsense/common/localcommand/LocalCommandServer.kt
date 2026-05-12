package tech.fastsense.common.localcommand

import android.util.Log
import com.google.gson.Gson
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpMethod
import io.ktor.server.application.install
import io.ktor.server.engine.EmbeddedServer
import io.ktor.server.engine.embeddedServer
import io.ktor.server.jetty.Jetty
import io.ktor.server.jetty.JettyApplicationEngine
import io.ktor.server.jetty.JettyApplicationEngineBase
import io.ktor.server.plugins.cors.routing.CORS
import io.ktor.server.routing.routing
import io.ktor.server.websocket.DefaultWebSocketServerSession
import io.ktor.server.websocket.WebSockets
import io.ktor.server.websocket.webSocket
import io.ktor.websocket.Frame
import io.ktor.websocket.close
import io.ktor.websocket.readText
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.joinAll
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import kotlin.coroutines.cancellation.CancellationException

class LocalCommandServer(private val port: Int = DEFAULT_PORT) {
    fun interface CommandListener {
        operator fun invoke(command: LocalCommand)
    }

    fun interface ClientCountListener {
        operator fun invoke(count: Int)
    }

    private var server: EmbeddedServer<JettyApplicationEngine, JettyApplicationEngineBase.Configuration>? =
        null
    private val clients = ConcurrentHashMap<UUID, Channel<LocalCommand>>()
    private val gson = Gson()

    var onReceivedCommand: CommandListener? = null
    var onClientCountChanged: ClientCountListener? = null

    val isRunning: Boolean
        get() = server != null

    fun start() {
        if (server != null) return
        try {
            val created = embeddedServer(Jetty, port = port) {
                install(WebSockets)
                install(CORS) {
                    anyHost()
                    allowMethod(HttpMethod.Get)
                    allowHeader(HttpHeaders.ContentType)
                }
                routing {
                    webSocket("/") {
                        val connectionId = UUID.randomUUID()
                        clients[connectionId] = Channel(Channel.UNLIMITED)
                        onClientCountChanged?.invoke(clients.size)
                        log("client connected: $connectionId")
                        try {
                            listOf(
                                async(Dispatchers.IO) { outputLoop(connectionId) },
                                async(Dispatchers.IO) { inputLoop(connectionId) },
                                async(Dispatchers.IO) { pingLoop() },
                            ).joinAll()
                        } catch (_: CancellationException) {
                        } catch (e: Exception) {
                            loge("connection $connectionId failed: $e")
                        } finally {
                            runCatching { close() }
                            clients.remove(connectionId)?.close()
                            onClientCountChanged?.invoke(clients.size)
                            log("client disconnected: $connectionId")
                        }
                    }
                }
            }
            created.start(wait = false)
            server = created
            log("server started on :$port")
        } catch (e: Exception) {
            server = null
            loge("failed to start server on :$port: $e")
            throw e
        }
    }

    fun stop() {
        val current = server ?: return
        runCatching {
            current.stop(250, 1000)
            clients.values.forEach { it.close() }
            clients.clear()
        }
        server = null
        onClientCountChanged?.invoke(0)
        log("server stopped")
    }

    suspend fun send(command: LocalCommand) {
        broadcast(command)
    }

    private suspend fun DefaultWebSocketServerSession.inputLoop(connectionId: UUID) {
        for (frame in incoming) {
            frame as? Frame.Text ?: continue
            val text = frame.readText()
            if (text == PING) continue
            try {
                val command = gson.fromJson(text, LocalCommand::class.java)
//                log("received from $connectionId: $command")
                onReceivedCommand?.invoke(command)
                broadcast(command)
            } catch (e: Exception) {
                loge("failed to parse command from $connectionId: $text")
            }
        }
    }

    private suspend fun DefaultWebSocketServerSession.outputLoop(connectionId: UUID) {
        for (command in clients[connectionId] ?: return) {
            send(Frame.Text(gson.toJson(command)))
        }
    }

    private suspend fun DefaultWebSocketServerSession.pingLoop() {
        while (true) {
            val result = this.outgoing.trySend(Frame.Text(PING))
            if (result.isFailure || result.isClosed) {
                throw IllegalStateException("Ping failed")
            }
            delay(1_000L)
        }
    }

    private suspend fun broadcast(command: LocalCommand) {
        clients.values.forEach { channel ->
            runCatching { channel.send(command) }
        }
    }

    companion object {
        const val DEFAULT_PORT = 8080
        private const val TAG = "LocalCommandServer"
        private const val PING = "ping"

        private fun log(message: String) = Log.i(TAG, message)
        private fun loge(message: String) = Log.e(TAG, message)
    }
}
