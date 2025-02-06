package tech.fastsense.common.nkf

import java.nio.ByteBuffer
import java.nio.ByteOrder

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