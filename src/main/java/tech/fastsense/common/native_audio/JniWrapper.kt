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

}