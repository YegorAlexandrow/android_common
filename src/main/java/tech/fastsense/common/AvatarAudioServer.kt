package tech.fastsense.head

import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import android.util.Log
import androidx.preference.PreferenceManager
import io.ktor.server.application.install
import io.ktor.server.engine.embeddedServer
import io.ktor.server.jetty.Jetty
import io.ktor.server.jetty.JettyApplicationEngine
import io.ktor.server.routing.routing
import io.ktor.server.websocket.DefaultWebSocketServerSession
import io.ktor.server.websocket.WebSockets
import io.ktor.server.websocket.webSocket
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
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import tech.fastsense.common.native_audio.JniWrapper
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread
import kotlin.coroutines.cancellation.CancellationException
import kotlin.math.abs
import kotlin.math.min
import kotlin.math.pow

class AvatarAudioServer(private val context: Context) : KoinComponent {

    private var audioServerThread: Thread? = null
    private var server: JettyApplicationEngine? = null
    private var audioTrack: AudioTrack? = null
    private var audioRecord: AudioRecord? = null
    private var recThread: Thread? = null
    private val recCoroChannel = Channel<ShortArray>(capacity = 256)
    private val renderCoroChannel = Channel<ShortArray>(capacity = 256)

    private val jni: JniWrapper by inject()

    private var audioStreamConnected = false

    private val sampleRate = 24000
    private val recSampleRate = 16000

    private val gccMargin = 4096
    private val gccSamples = gccMargin * 16
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

            if (indexInInput < input24kHz.size) {
                val sampleWithGain = input24kHz[indexInInput] * gainFactor
                output16kHz[i] =
                    (sampleWithGain.coerceIn(-1.0, 1.0) * Short.MAX_VALUE).toInt().toShort()
            } else {
                output16kHz[i] = 0
            }
        }

        return output16kHz
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
            val chunk = frame.readBytes().floatArray

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

            val resampledChunk = convertAndResample(
                chunk,
                inputSampleRate,
                outputSampleRate
            )
            renderCoroChannel.send(resampledChunk)
        }
    }

    //todo obj decomp
    @OptIn(DelicateCoroutinesApi::class)
    private suspend fun DefaultWebSocketServerSession.outputChunks() {
        val bufferX = ByteBuffer.allocateDirect(gccSamples * 2).order(endian)
        val bufferY = ByteBuffer.allocateDirect(gccSamples * 2).order(endian)

        var driftTimeoutRunning = false

        var batchChunkIndex = 0
        val batchSamples = samplesInBatchChunk * batchSize
        val renderBatch = ShortArray(batchSamples)
        val recordBatch = ShortArray(batchSamples)
        val renderBytes = Stb(batchSamples)
        val recordBytes = Stb(batchSamples)

        suspend fun batchLoop(renderChunk: ShortArray, recordChunk: ShortArray) {
            renderChunk.forEachIndexed { i, it ->
                renderBatch[batchChunkIndex * samplesInBatchChunk + i] = it
            }
            recordChunk.forEachIndexed { i, it ->
                recordBatch[batchChunkIndex * samplesInBatchChunk + i] = it
            }
            if (++batchChunkIndex != batchSize) return
            send(renderBytes(renderBatch))
            send(recordBytes(recordBatch))
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
            if (drifting || isSilence(renderChunk)) return
            for (i in renderChunk.indices) {
                if (bufferX.position() >= bufferX.capacity()) break
                bufferX.putShort(renderChunk[i])
                bufferY.putShort(recChunk[i])
            }
            if (bufferX.position() < bufferX.capacity()) return
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
            if (renderChunk == null || recChunk == null) return
            batchLoop(renderChunk, recChunk)
            gccLoop(renderChunk, recChunk)
        }

        while (!renderCoroChannel.isClosedForReceive && !recCoroChannel.isClosedForReceive) loop()
    }

    fun start() {
        val settings = PreferenceManager.getDefaultSharedPreferences(context)
        if (!settings.getBoolean("use_speaker", false) || audioServerThread != null) return

        initAudioTrack()
        initAudioRecord()
        jni.gccPhatInit(gccSamples)

        audioServerThread = thread {
            server = embeddedServer(Jetty, port = 8080) {
                install(WebSockets)
                routing {
                    webSocket("/") {
                        log("audio ws connected")
                        try {
                            if (audioStreamConnected) throw RuntimeException("audio stream socket already connected")
                            audioStreamConnected = true
                            listOf(
                                async(Dispatchers.IO) { inputChunks() },
                                async(Dispatchers.IO) { outputChunks() }
                            ).joinAll()
                        } catch (_: CancellationException) {
                            log("canceled")
                        } catch (e: Exception) {
                            log("audio ws got exception: $e")
                        } finally {
                            log("audio ws closed")
                            this.close()
                            audioStreamConnected = false
                            audioTrack!!.stop()
                            audioTrack!!.release()
                            audioRecord!!.stop()
                            audioRecord!!.release()
                        }
                    }
                }
            }.start()
        }
    }

    private fun isSilence(renderChunk: ShortArray): Boolean {
        for (i in renderChunk.indices step samplesInBatchChunk / 50) {
            if (renderChunk[i] != 0.toShort()) return false
        }
        return true
    }

    fun stop() {
        if (audioServerThread == null) return
        log("stop")
        server!!.stop(150, 250)
        audioServerThread!!.interrupt()
        audioServerThread!!.join()
        server = null
        audioTrack = null
        audioRecord = null
        recThread = null
        audioServerThread = null
        audioRecordStarted = false
        log("audio server finished")
    }

    class Ftb(size: Int) {
        private val buf: ByteBuffer = ByteBuffer.allocate(size * 4).order(endian)
        operator fun invoke(fa: FloatArray): ByteArray = with(buf) {
            position(0)
            for (float in fa) putFloat(float)
            array()
        }
    }

    class Stb(size: Int) {
        private val buf: ByteBuffer = ByteBuffer.allocate(size * 2).order(endian)
        operator fun invoke(sa: ShortArray): ByteArray = with(buf) {
            position(0)
            for (short in sa) putShort(short)
            array()
        }
    }

    companion object {
        const val TAG = "AvatarAudioServer"
        fun log(s: String) = Log.e(TAG, s)

        val endian: ByteOrder = ByteOrder.LITTLE_ENDIAN

        fun Int.samplesToMs(sampleRate: Int): Double {
            return this / (sampleRate / 1000.0)
        }

        val ByteArray.floatArray: FloatArray
            get() {
                val floatBuffer =
                    ByteBuffer.wrap(this).order(endian).asFloatBuffer()
                val floatArray = FloatArray(floatBuffer.remaining())
                floatBuffer.get(floatArray)
                return floatArray
            }
    }
}