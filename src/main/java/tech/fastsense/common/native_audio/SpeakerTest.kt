package tech.fastsense.common.native_audio

interface SpeakerTest {
    fun interface OnResult {
        operator fun invoke(result: Boolean)
    }

    fun start(onResult: OnResult)
    fun getDeviceInfo(): Map<String, Any>
}