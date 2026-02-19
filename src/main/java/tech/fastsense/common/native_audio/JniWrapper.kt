package tech.fastsense.common.native_audio

import android.content.res.AssetManager
import com.opentok.android.BaseAudioDevice
import tech.fastsense.common.cloud_storage.CloudStorageApi
import java.nio.ByteBuffer

class JniWrapper {
    companion object {
        init {
            System.loadLibrary("native_audio")
        }
    }

    interface RenderCallback {
        fun run(underrunCount: Int): ShortArray
    }

    interface CaptureCallback {
        fun run(data: ByteArray)
    }

    external fun setupTest(assets: AssetManager, cb: NativeSpeakerTest.JniCallback, name: String)
    external fun setupStorage(storage: CloudStorageApi)
    external fun runAudioSystemTest()

    external fun setupCall(bus: BaseAudioDevice.AudioBus)
    external fun startCall()
    external fun stopCall()

    external fun setAudioBusRenderState(state: Boolean)
    external fun setAudioBusCaptureState(state: Boolean)

    external fun gccPhatInit(size: Int)
    external fun gccPhatExecute(x: ByteBuffer, y: ByteBuffer, margin: Int): Int

    external fun createAec(
        reference: Int, input: Int,
        processing: Int, output: Int
    ): Long

    external fun destroyAec(handle: Long)
    external fun aecProcessCapture(
        handle: Long,
        captureFrame: ShortArray,
        additionalGain: Float = -1f, // applies only if >0f
    ): ShortArray?

    external fun aecGetDelay(handle: Long): Int

    external fun oboeCreateRecorder(sampleRate: Int, framesPerBuffer: Int, id: Int): Long
    external fun oboeDestroyRecorder(handle: Long)
    external fun oboeStartRecording(handle: Long, captureCallback: CaptureCallback)
    external fun oboeStopRecording(handle: Long)

    external fun oboeCreateRenderer(sampleRate: Int, framesPerBuffer: Int): Long
    external fun oboeDestroyRenderer(handle: Long)
    external fun oboeStartRendering(handle: Long, aecHandle: Long, renderCallback: RenderCallback)
    external fun oboeStopRendering(handle: Long)

    external fun oboeIsPlaying(handle: Long): Boolean
    external fun initSpectro(n: Int)
    external fun runSpectro(
        shorts: ShortArray,
        magnitudes: FloatArray,
        dbRange: Double,
        dbGain: Double
    )

    external fun pushRenderBuffer(handle: Long): Boolean
    external fun popReferenceBuffer(handle: Long): Boolean
    external fun getRenderBuffer(handle: Long): ByteBuffer
    external fun getReferenceBuffer(handle: Long): ByteBuffer
}