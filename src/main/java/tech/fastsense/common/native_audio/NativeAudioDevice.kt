package tech.fastsense.common.native_audio

import android.content.Context
import com.opentok.android.BaseAudioDevice
import org.koin.java.KoinJavaComponent

class NativeAudioDevice(private val context: Context) : BaseAudioDevice() {
    private val jni: JniWrapper by KoinJavaComponent.inject(JniWrapper::class.java)

    private var inited = false
    private var initLock = Any()


    private var state = false
    private var stateLock = Any()

    private fun dispatchInit() {
        synchronized(initLock) {
            if (!inited) {
                jni.setupCall(audioBus)
                inited = true
            }
        }
    }

    private fun dispatchState(s: Boolean) {
        synchronized(stateLock) {
            if (!state) {
                if (s) {
                    jni.startCall()
                    state = true
                }
            } else {
                if (!s) {
                    jni.stopCall()
                    state = false
                }
            }
        }
    }

    override fun initCapturer(): Boolean {
        dispatchInit()
        return true
    }

    override fun initRenderer(): Boolean {
        dispatchInit()
        return true
    }

    override fun startCapturer(): Boolean {
        dispatchState(true)
        jni.setAudioBusCaptureState(true)
        return true
    }

    override fun startRenderer(): Boolean {
        dispatchState(true)
        jni.setAudioBusRenderState(true)
        return true
    }

    override fun stopCapturer(): Boolean {
        dispatchState(false)
        jni.setAudioBusCaptureState(false)
        return true
    }

    override fun stopRenderer(): Boolean {
        dispatchState(false)
        jni.setAudioBusRenderState(false)
        return true
    }

    override fun destroyCapturer(): Boolean {
        return true
    }

    override fun destroyRenderer(): Boolean {
        return true
    }

    override fun getEstimatedCaptureDelay(): Int {
        return 0
    }

    override fun getEstimatedRenderDelay(): Int {
        return 0
    }

    override fun getCaptureSettings(): AudioSettings {
        return AudioSettings(48000, 1)
    }

    override fun getRenderSettings(): AudioSettings {
        return AudioSettings(48000, 1)
    }

    override fun onPause() {
    }

    override fun onResume() {
    }
}

