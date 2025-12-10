@file:OptIn(DelicateCoroutinesApi::class)

package tech.fastsense.common.nkf

import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import android.os.Environment
import android.util.Log
import io.ktor.client.HttpClient
import io.ktor.client.plugins.websocket.webSocket
import io.ktor.server.application.install
import io.ktor.server.engine.embeddedServer
import io.ktor.server.jetty.Jetty
import io.ktor.server.jetty.JettyApplicationEngine
import io.ktor.server.routing.routing
import io.ktor.server.websocket.DefaultWebSocketServerSession
import io.ktor.server.websocket.WebSockets
import io.ktor.server.websocket.webSocket
import io.ktor.websocket.DefaultWebSocketSession
import io.ktor.websocket.Frame
import io.ktor.websocket.close
import io.ktor.websocket.readBytes
import io.ktor.websocket.send
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import tech.fastsense.common.native_audio.JniWrapper
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread
import kotlin.coroutines.cancellation.CancellationException
import kotlin.math.abs
import kotlin.math.min
import kotlin.math.pow
import io.ktor.client.plugins.websocket.WebSockets as ClientWebSockets

class AvatarAudioServer(private val context: Context) : KoinComponent {

    private var server: JettyApplicationEngine? = null
    private var audioTrack: AudioTrack? = null
    private var audioRecord: AudioRecord? = null
    private var recThread: Thread? = null
    private val recCoroChannel = Channel<ShortArray>(capacity = 256)
    private val renderCoroChannel = Channel<ShortArray>(capacity = 256)

    private val jni: JniWrapper by inject()

    private val filePath =
        context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)?.absolutePath + "/nkf_test_"

    private var writeDbgFiles = true
    private var spk24fFos: FileOutputStream? = null
    private var spkFos: FileOutputStream? = null
    private var micFos: FileOutputStream? = null
    private var resFos: FileOutputStream? = null

    private var audioStreamConnected = false
    private var continuousGcc = false

    private val sampleRate = 24000
    private val recSampleRate = 16000

    private val gccMargin = 4096
    private val gccSamples = gccMargin * 2
    private val compensationLimit = 80

    private val batchSize = 16
    private val samplesInBatchChunk =
        320 //todo match incoming size (in 480-> out 320, 240->160,etc) (better converter?)

    private val dbGain = 8.5
    private val gainFactor = 10.0.pow(dbGain / 20.0)

    private val audioFormat = AudioFormat.ENCODING_PCM_FLOAT
    private val recAudioFormat = AudioFormat.ENCODING_PCM_16BIT

    private val _drift = AtomicInteger(0)

    private val _drifting = AtomicBoolean(false)
    private var drifting
        get() = _drifting.get()
        set(value) {
            _drifting.set(value)
        }

    private var exactDrift: Int
        get() = _drift.get()
        set(value) {
            _drift.set(value)
        }

    private var audioRecordStarted = false
    private val recAudioSource = MediaRecorder.AudioSource.MIC
    private val recChannelConfig = AudioFormat.CHANNEL_IN_MONO
    private val recBufferSize =
        AudioRecord.getMinBufferSize(recSampleRate, recChannelConfig, recAudioFormat)

    private fun isChunkSizeValid(
        inputSize: Int,
        inputSampleRate: Int,
        outputSampleRate: Int
    ): Boolean {
        val resamplingFactor = inputSampleRate.toDouble() / outputSampleRate.toDouble()
        val outputSize = (inputSize / resamplingFactor).toInt()
        val recalculatedInputSize = (outputSize * resamplingFactor).toInt()
        return recalculatedInputSize == inputSize
    }

    private fun convertAndResample(
        input24kHz: FloatArray,
        inputSampleRate: Int,
        outputSampleRate: Int
    ): ShortArray {
        val resamplingFactor = inputSampleRate.toDouble() / outputSampleRate.toDouble()
        val output16kHzSize = (input24kHz.size / resamplingFactor).toInt()
        val output16kHz = ShortArray(output16kHzSize)

        for (i in output16kHz.indices) {
            val indexInInput = (i * resamplingFactor).toInt()
            val filtered = lowPassFilter(input24kHz, inputSampleRate, outputSampleRate / 2)
            if (indexInInput < filtered.size) {
                val sampleWithGain = filtered[indexInInput] * gainFactor
                output16kHz[i] =
                    (sampleWithGain.coerceIn(-1.0, 1.0) * Short.MAX_VALUE).toInt().toShort()
            } else {
                output16kHz[i] = 0
            }
        }
        return output16kHz
    }

    private fun lowPassFilter(
        input: FloatArray,
        sampleRate: Int,
        cutoffFrequency: Int
    ): FloatArray {
        val rc = 1.0 / (2.0 * Math.PI * cutoffFrequency)
        val dt = 1.0 / sampleRate
        val alpha = dt / (rc + dt)

        val output = FloatArray(input.size)
        output[0] = input[0]

        for (i in 1 until input.size) {
            output[i] = (output[i - 1] + alpha * (input[i] - output[i - 1])).toFloat()
        }

        return output
    }

    private fun lowPassFilter(
        input: ShortArray,
        sampleRate: Int,
        cutoffFrequency: Int
    ): ShortArray {
        val rc = 1.0 / (2.0 * Math.PI * cutoffFrequency)
        val dt = 1.0 / sampleRate
        val alpha = dt / (rc + dt)

        val output = ShortArray(input.size)
        output[0] = input[0]

        for (i in 1 until input.size) {
            val filtered = output[i - 1] + alpha * (input[i] - output[i - 1])
            output[i] = filtered.toInt().toShort()
        }

        return output
    }

    private fun initAudioTrack() {
        val channelConfig = AudioFormat.CHANNEL_OUT_MONO
        val bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat)
        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(audioFormat)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelConfig)
                    .build()
            )
            .setBufferSizeInBytes(bufferSize)
            .build()
        if (audioTrack!!.state != AudioTrack.STATE_INITIALIZED) throw RuntimeException("audioTrack not initialized")
    }

    @SuppressLint("MissingPermission")
    private fun initAudioRecord() {
        audioRecord = AudioRecord.Builder()
            .setAudioSource(recAudioSource)
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(recAudioFormat)
                    .setSampleRate(recSampleRate)
                    .setChannelMask(recChannelConfig)
                    .build()
            )
            .setBufferSizeInBytes(recBufferSize)
            .build()
        if (audioRecord!!.state != AudioRecord.STATE_INITIALIZED) throw RuntimeException("audioRecord not initialized")
    }

    private fun startAudioRecord() {
        if (audioRecordStarted) return
        audioTrack!!.play()
        audioRecordStarted = true
        audioRecord!!.startRecording()
        recThread = thread {
            runBlocking {
                val data = ShortArray(samplesInBatchChunk)
                val drifters = ShortArray(compensationLimit)
                while (audioStreamConnected) {
                    audioRecord?.let {
                        val curDrift = exactDrift
                        val comp = min(compensationLimit, abs(curDrift))
                        if (curDrift > 0) {
                            log("positive drift, skipping record samples")
                            exactDrift -= comp
                            it.read(drifters, 0, comp, AudioRecord.READ_BLOCKING)
                        }
                        it.read(data, 0, data.size, AudioRecord.READ_BLOCKING)
                        recCoroChannel.send(data)
                    }
                }
            }
        }
    }

    private suspend fun DefaultWebSocketServerSession.inputChunks() {
        val inputSampleRate = sampleRate
        val outputSampleRate = recSampleRate
        for (frame in incoming) {
            startAudioRecord()
            val chunkRaw = frame.readBytes()
            val chunk = chunkRaw.floatArray

            if (!isChunkSizeValid(chunk.size, inputSampleRate, outputSampleRate)) {
                log("Invalid chunk size for resampling")
                break
            }

            audioTrack!!.write(
                chunk,
                0,
                chunk.size,
                AudioTrack.WRITE_BLOCKING
            )

            if (exactDrift < 0) {
                log("negative drift, skipping render chunk gcc buffering")
                exactDrift += samplesInBatchChunk
                continue
            }
            if (writeDbgFiles) withContext(Dispatchers.IO) { spk24fFos?.write(chunkRaw) }
            val resampledChunk = convertAndResample(
                chunk,
                inputSampleRate,
                outputSampleRate
            )
            renderCoroChannel.send(resampledChunk)
        }
    }
    private suspend fun DefaultWebSocketSession.sendMic(serverSession: DefaultWebSocketServerSession) {
        for (frame in incoming) {
            log("new mic frame")
            try {
                serverSession.send(frame)
                if (writeDbgFiles) withContext(Dispatchers.IO) { resFos?.write(frame.readBytes()) }
            } catch (_: CancellationException) {
                log("canceled")
            } catch (e: Exception) {
                log("incoming error: " + e.stackTraceToString())
            }
        }
    }

    private suspend fun DefaultWebSocketSession.sendNkf() {
        val bufferX = ByteBuffer.allocateDirect(gccSamples * 2).order(endian)
        val bufferY = ByteBuffer.allocateDirect(gccSamples * 2).order(endian)

        val cutoffFrequency = 4000
        val recordFilter = ButterworthLowPassFilter(recSampleRate, cutoffFrequency)
        val renderFilter = ButterworthLowPassFilter(recSampleRate, cutoffFrequency)

        var firstDriftFound = false
        var silenceSkipped = false
        var driftTimeoutRunning = false

        var batchChunkIndex = 0
        val batchSamples = samplesInBatchChunk * batchSize
        val renderBatch = ShortArray(batchSamples)
        val recordBatch = ShortArray(batchSamples)
        val renderBytes = Stb(batchSamples)
        val recordBytes = Stb(batchSamples)

        suspend fun batchLoop(renderChunk: ShortArray, recordChunk: ShortArray) {
            val filteredRender = renderFilter.process(renderChunk)
            val filteredRecord = recordFilter.process(recordChunk)

            filteredRender.forEachIndexed { i, it ->
                renderBatch[batchChunkIndex * samplesInBatchChunk + i] = it
            }
            filteredRecord.forEachIndexed { i, it ->
                recordBatch[batchChunkIndex * samplesInBatchChunk + i] = it
            }
            if (++batchChunkIndex != batchSize) return

//            data class Meta(val spk_size: Int, val mic_size: Int, val ts: Long)
            val f = Frame.Text(
                "{\"spk_size\": $batchSamples,\"mic_size\": $batchSamples,\"ts\": ${System.currentTimeMillis()} }"
            )
            send(f)
            val render = renderBytes(renderBatch)
            val record = recordBytes(recordBatch)
            send(render)
            send(record)
            if (writeDbgFiles) withContext(Dispatchers.IO) {
                spkFos?.write(render)
                micFos?.write(record)
            }

            batchChunkIndex = 0
        }

        fun gccLoop(renderChunk: ShortArray, recChunk: ShortArray) {
            if (drifting && exactDrift == 0 && !driftTimeoutRunning) {
                log("drift timeout")
                driftTimeoutRunning = true
                launch {
                    bufferX.position(0)
                    bufferY.position(0)
                    delay(5000)
                    log("~drift timeout")
                    drifting = false
                    driftTimeoutRunning = false
                }
            }
            if (drifting) return
            if (!silenceSkipped) {
                if (isSilence(renderChunk)) return
                silenceSkipped = true
            }
            for (i in renderChunk.indices) {
                if (bufferX.position() >= bufferX.capacity()) break
                bufferX.putShort(renderChunk[i])
                bufferY.putShort(recChunk[i])
            }
            if (bufferX.position() < bufferX.capacity()) return
            firstDriftFound = true
            log("GCC-PHAT RUN")
            bufferX.position(0)
            bufferY.position(0)
            val result = jni.gccPhatExecute(bufferX, bufferY, gccMargin)
            exactDrift = result
            if (exactDrift != 0) drifting = true
            log("Result from GCC-PHAT: $result samples or ${result.samplesToMs(recSampleRate)} ms")
        }

        suspend fun loop() {
            val renderChunk = renderCoroChannel.receiveCatching().getOrNull()
            val recChunk = recCoroChannel.receiveCatching().getOrNull()
            val noRender = renderChunk == null
            val noRec = recChunk == null
            if (noRender || noRec) {
                log("noRender: $noRender || noRec: $noRec")
                return
            }
            batchLoop(renderChunk!!, recChunk!!)
            if (!firstDriftFound || continuousGcc) gccLoop(renderChunk, recChunk)
        }

        while (!renderCoroChannel.isClosedForReceive && !recCoroChannel.isClosedForReceive) loop()

    }

    //todo obj decomp
    private suspend fun DefaultWebSocketServerSession.handleClient() {

        val client = HttpClient {
            install(ClientWebSockets)
        }
        client.webSocket(
            host = "34.34.10.219",
            port = 8000,
            path = "/ws",
        ) {
            try {
                listOf(
                    async(Dispatchers.IO) { sendMic(this@handleClient) },
                    async(Dispatchers.IO) { sendNkf() }
                ).joinAll()
            } catch (e: Exception) {
                log("exception: $e")
            } finally {
                this.close()
            }
        }


    }

    fun start() {
        log("START")
//  todo decouple wehead prefs
//        val settings = PreferenceManager.getDefaultSharedPreferences(context)
//        if (!settings.getBoolean("use_speaker", false) || audioServerThread != null) return
        if (server != null) return
        log("INIT")

        initAudioTrack()
        initAudioRecord()
        jni.gccPhatInit(gccSamples)
//        server = embeddedServer(Jetty, port = 8082) {
//            install(WebSockets)
//            routing {
//                webSocket("/") {
//                    val time = System.currentTimeMillis()
//                    log("audio ws connected at: $time")
//                    try {
//                        if (writeDbgFiles) {
//                            spk24fFos = FileOutputStream(filePath + "spk24f_" + time)
//                            spkFos = FileOutputStream(filePath + "spk_" + time)
//                            micFos = FileOutputStream(filePath + "mic_" + time)
//                            resFos = FileOutputStream(filePath + "res_" + time)
//                        }
//                        if (audioStreamConnected) throw RuntimeException("audio stream socket already connected")
//                        audioStreamConnected = true
//                        listOf(
//                            async(Dispatchers.IO) { inputChunks() },
//                            async(Dispatchers.IO) { handleClient() },
//                        ).joinAll()
//                    } catch (_: CancellationException) {
//                        log("canceled")
//                    } catch (e: Exception) {
//                        log("audio ws got exception: $e")
//                    } finally {
//                        log("audio ws closed")
//                        this.close()
//                        audioStreamConnected = false
//                        audioTrack!!.stop()
//                        audioTrack!!.release()
//                        audioRecord!!.stop()
//                        audioRecord!!.release()
//
//                        spk24fFos?.flush()
//                        spkFos?.flush()
//                        micFos?.flush()
//                        resFos?.flush()
//                        spk24fFos?.close()
//                        spkFos?.close()
//                        micFos?.close()
//                        resFos?.close()
//                    }
//                }
//            }
//        }.start(wait = false)
    }


    private fun isSilence(renderChunk: ShortArray): Boolean {
        for (i in renderChunk.indices step samplesInBatchChunk / 50) {
            if (renderChunk[i] != 0.toShort()) return false
        }
        return true
    }

    fun stop() {
        if (server == null) return
        log("stop")
        server!!.stop(150, 250)
        server = null
        audioTrack = null
        audioRecord = null
        recThread = null
        audioRecordStarted = false
        log("audio server finished")
    }

    companion object {
        private const val TAG = "NkfAudioServer"
        private fun log(m: String) = Log.v(TAG, m)
        private fun loge(m: String) = Log.e(TAG, m)
    }
}