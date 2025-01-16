package tech.fastsense.common.native_audio

class DummySpeakerTest : SpeakerTest {
    override fun start(onResult: SpeakerTest.OnResult) {
    }

    override fun getDeviceInfo() = mapOf("test_result" to "native audio disabled or not supported")
}