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
    external fun aecProcessFrame(
        handle: Long,
        renderFrame: ShortArray,
        captureFrame: ShortArray,
        additionalGain: Float = -1f // applies only if >0f
    ): ShortArray?

    external fun oboeCreateRecorder(sampleRate: Int, framesPerBuffer: Int, id: Int): Long
    external fun oboeDestroyRecorder(handle: Long)
    external fun oboeStartRecording(handle: Long)
    external fun oboeStopRecording(handle: Long)
    external fun oboeRead(handle: Long, buffer: ByteArray): Long

    external fun oboeCreateRenderer(sampleRate: Int, framesPerBuffer: Int): Long
    external fun oboeDestroyRenderer(handle: Long)
    external fun oboeStartRendering(handle: Long)
    external fun oboeStopRendering(handle: Long)
    external fun oboeWrite(handle: Long, buffer: ShortArray)

    external fun oboeIsPlaying(handle: Long): Boolean

}