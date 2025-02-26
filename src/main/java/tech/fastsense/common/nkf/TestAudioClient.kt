package tech.fastsense.common.nkf

import android.content.Context
import android.os.Environment
import android.util.Log
import io.ktor.client.HttpClient
import io.ktor.client.plugins.websocket.DefaultClientWebSocketSession
import io.ktor.client.plugins.websocket.WebSockets
import io.ktor.client.plugins.websocket.webSocket
import io.ktor.websocket.close
import io.ktor.websocket.readBytes
import io.ktor.websocket.send
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.delay
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import kotlin.concurrent.thread

class TestAudioClient(context: Context) {

    private var client: HttpClient? = null
    private var thread: Thread? = null

    private fun readAudioFileInChunks(file: File, chunkSize: Int): List<FloatArray> {
        val chunkList = mutableListOf<FloatArray>()
        val byteArray = file.readBytes()
        val totalFloats = byteArray.size / 4
        val byteBuffer = ByteBuffer.wrap(byteArray).order(endian)
        for (i in 0 until totalFloats step chunkSize) {
            val currentChunkSize = minOf(chunkSize, totalFloats - i)
            val chunk = FloatArray(currentChunkSize)
            for (j in 0 until currentChunkSize) {
                chunk[j] = byteBuffer.float
            }
            chunkList.add(chunk)
        }

        return chunkList
    }

    private suspend fun DefaultClientWebSocketSession.output() {
        val samplesInChunk = 480
        val testChunks = readAudioFileInChunks(
            File("/storage/emulated/0/Download/avatar_test_voice_raw_24_f32.mp3"),
            samplesInChunk
        )
        val ftb = Ftb(samplesInChunk)
        while (true) {
            testChunks.forEach {
                send(ftb(it))
                delay(5)
            }
        }
    }

    private val filePath =
        context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)?.absolutePath + "/AUDIO_TEST_"

    private suspend fun DefaultClientWebSocketSession.input(micFos: FileOutputStream) {
        for (frame in incoming) {
            log("new mic frame")
            try {
                val mic = frame.readBytes()
                withContext(Dispatchers.IO) { micFos.write(mic) }
            } catch (e: Exception) {
                log("incoming error: " + e.stackTraceToString())
                break
            }
        }
    }

    fun start() {
        loge("start")
        if (client != null) return
        loge("thread launch")
        thread = thread {
            loge("thread")
            client = HttpClient { install(WebSockets) }
            val micFos = FileOutputStream(filePath + "mic_" + System.currentTimeMillis())
            try {
                runBlocking {
                    client?.webSocket(host = "localhost", port = 8082, path = "/") {
                        try {
                            listOf(
                                async(Dispatchers.IO) { input(micFos) },
                                async(Dispatchers.IO) { output() },
                            ).joinAll()
                        } catch (e: Exception) {
                            loge("exception: $e")
                        } finally {
                            this.close()
                        }
                    }
                }
            } catch (e: Exception) {
                if (client == null) return@thread
                val waitFor = 1000L
                loge("failed trying in $waitFor ms")
                try {
                    Thread.sleep(waitFor)
                } catch (e: InterruptedException) {
                    return@thread
                } finally {
                    micFos.flush()
                    micFos.close()
                }
            }
            loge("~thread")
        }
    }

    fun stop() {
        log("stop")
        client?.close()
        log("closed")
        client = null
        thread?.interrupt()
        thread?.join()
        log("joined")
        thread = null
    }

    companion object {
        private const val TAG = "TestAudioClient"
        private fun log(m: String) = Log.v(TAG, m)
        private fun loge(m: String) = Log.e(TAG, m)
    }
}