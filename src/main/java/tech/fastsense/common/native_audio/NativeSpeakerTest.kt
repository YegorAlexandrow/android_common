package tech.fastsense.common.native_audio

import android.content.Context
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.util.Log
import androidx.preference.PreferenceManager
import org.koin.java.KoinJavaComponent
import tech.fastsense.common.cloud_storage.CloudStorageApi

class NativeSpeakerTest(private val context: Context) : SpeakerTest { // c++: SpeakerTest.h
    var onResult: SpeakerTest.OnResult? = null
    private val jni: JniWrapper by KoinJavaComponent.inject(JniWrapper::class.java)
    private val audioInfo = mutableMapOf<String, Any>()

    init {
        jni.setupStorage(KoinJavaComponent.get(CloudStorageApi::class.java))
        jni.setupTest(
            context.assets,
            JniCallback(this, context, 0.5, 0.2),
            "keys.raw"
        )
    }

    class TestResult { // c++: SpeakerTestResult.h
        class DeviceInfo { // c++: DeviceInfo.h
            var api: String = "-"
            var id: Int = -1
            var bytesPerSample: Int = -1
            var bytesPerFrame: Int = -1
            var framesPerBurst: Int = -1
            var framesPerDataCallback: Int = -1
            var sampleRate: Int = -1
        }

        var speakerInfo = DeviceInfo()
        var microInfo = DeviceInfo()

        var timestamp: Long = 0L
        var form: Double = -1.0
        var scale: Double = -1.0


        fun fillMicroInfo(
            api_: String,
            id_: Int,
            bytesPerSample_: Int,
            bytesPerFrame_: Int,
            framesPerBurst_: Int,
            framesPerDataCallback_: Int,
            sampleRate_: Int
        ) {
            with(microInfo) {
                api = api_
                id = id_
                bytesPerSample = bytesPerSample_
                bytesPerFrame = bytesPerFrame_
                framesPerBurst = framesPerBurst_
                framesPerDataCallback = framesPerDataCallback_
                sampleRate = sampleRate_
            }
        }

        fun fillSpeakerInfo(
            api_: String,
            id_: Int,
            bytesPerSample_: Int,
            bytesPerFrame_: Int,
            framesPerBurst_: Int,
            framesPerDataCallback_: Int,
            sampleRate_: Int
        ) {
            with(speakerInfo) {
                api = api_
                id = id_
                bytesPerSample = bytesPerSample_
                bytesPerFrame = bytesPerFrame_
                framesPerBurst = framesPerBurst_
                framesPerDataCallback = framesPerDataCallback_
                sampleRate = sampleRate_
            }
        }

        fun fillResult(
            timestamp_: Long,
            form_: Double,
            scale_: Double
        ) {
            timestamp = timestamp_
            form = form_
            scale = scale_
        }
    }

    class JniCallback(
        private var test: NativeSpeakerTest,
        private val context: Context,
        private val formThreshold: Double,
        private val scaleThreshold: Double,
    ) {
        private var testResult = TestResult()

        fun getTestResultDestination() = testResult

        fun onTestResultReady() {
            val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val getManagerDevice = { id: Int ->
                audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS or AudioManager.GET_DEVICES_OUTPUTS)
                    .find { it.id == id }
            }

            var isDeviceTypeCorrect = true

            val fillDeviceInfo: (TestResult.DeviceInfo) -> Map<String, Any> = {
                val device = getManagerDevice(it.id)
                isDeviceTypeCorrect =
                    isDeviceTypeCorrect &&
                            ((device?.type ?: -1) == AudioDeviceInfo.TYPE_WIRED_HEADSET)
                mapOf(
                    "burst_frames" to it.framesPerBurst,
                    "callback_frames" to it.framesPerDataCallback,
                    "frame_size" to it.bytesPerFrame,
                    "sample_size" to it.bytesPerSample,
                    "sample_rate" to it.sampleRate,
                    "api" to it.api,
                    "device_type" to deviceTypeToStr(device?.type ?: -1),
                    "device_name" to (device?.productName ?: "UNKNOWN"),
                )
            }

            test.audioInfo["speaker"] = fillDeviceInfo(testResult.speakerInfo)
            test.audioInfo["micro"] = fillDeviceInfo(testResult.microInfo)

            val testPassed =
                testResult.form > formThreshold && testResult.scale > scaleThreshold && isDeviceTypeCorrect

            test.audioInfo["test_result"] = testResult.let {
                mutableMapOf<String, Any>(
                    "timestamp" to it.timestamp,
                    "form" to it.form,
                    "scale" to it.scale,
                    "test_passed" to testPassed,
                )
            }
            Log.v("NativeSpeakerTest.JniCallback", test.audioInfo.entries.joinToString("\n"))

            test.onResult?.invoke(testPassed)
            testResult = TestResult()
            test.restoreConfiguredVolume()
        }
    }

    override fun start(onResult: SpeakerTest.OnResult) {
        this.onResult = onResult
        forceTestVolume()
        jni.runAudioSystemTest()
    }

    override fun getDeviceInfo(): Map<String, Any> = audioInfo

    private fun forceTestVolume() {
        val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager

        am.setStreamVolume(
            AudioManager.STREAM_VOICE_CALL,
            am.getStreamMinVolume(AudioManager.STREAM_VOICE_CALL),
            0
        )

        am.setStreamVolume(
            AudioManager.STREAM_MUSIC,
            am.getStreamMaxVolume(AudioManager.STREAM_MUSIC),
            0
        )
    }

    private fun restoreConfiguredVolume() {
        val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager

        val mSettings = PreferenceManager.getDefaultSharedPreferences(context)
        val vol = mSettings.getString("audio_vol", "100")!!.toInt()

        am.setStreamVolume(
            AudioManager.STREAM_VOICE_CALL,
            am.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL),
            0
        )

        am.setStreamVolume(
            AudioManager.STREAM_MUSIC,
            am.getStreamMaxVolume(AudioManager.STREAM_MUSIC) * vol / 100,
            0
        )
    }

    companion object {
        fun deviceTypeToStr(type: Int) = when (type) {
            AudioDeviceInfo.TYPE_WIRED_HEADSET -> {
                "WIRED_HEADSET"
            }

            AudioDeviceInfo.TYPE_WIRED_HEADPHONES -> {
                "WIRED_HEADPHONES"
            }

            AudioDeviceInfo.TYPE_BUILTIN_MIC -> {
                "BUILTIN_MIC"
            }

            AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> {
                "BUILTIN_SPEAKER"
            }

            AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE -> {
                "BUILTIN_SPEAKER_SAFE"
            }

            else -> {
                "UNKNOWN"
            }
        }
    }
}