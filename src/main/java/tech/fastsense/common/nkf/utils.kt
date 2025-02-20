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

class BiquadFilter(a0: Float, a1: Float, a2: Float, b0: Float, b1: Float, b2: Float) {
    private var x1 = 0f
    private var x2 = 0f
    private var y1 = 0f
    private var y2 = 0f

    private val a0Inv = 1f / a0
    private val a1 = a1 * a0Inv
    private val a2 = a2 * a0Inv
    private val b0 = b0 * a0Inv
    private val b1 = b1 * a0Inv
    private val b2 = b2 * a0Inv

    fun process(input: ShortArray): ShortArray {
        val output = ShortArray(input.size)
        for (i in input.indices) {
            val x0 = input[i].toFloat()
            val y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2

            x2 = x1
            x1 = x0
            y2 = y1
            y1 = y0

            output[i] = y0.toInt().coerceIn(Short.MIN_VALUE.toInt(), Short.MAX_VALUE.toInt()).toShort()
        }
        return output
    }
}

class ButterworthLowPassFilter(sampleRate: Int, cutoffFrequency: Int) {
    private val biquads: Array<BiquadFilter>

    init {
        biquads = designButterworthLowPass(sampleRate.toFloat(), cutoffFrequency.toFloat())
    }

    fun process(input: ShortArray): ShortArray {
        var output = input
        for (biquad in biquads) {
            output = biquad.process(output)
        }
        return output
    }

    private fun designButterworthLowPass(sampleRate: Float, cutoffFreq: Float): Array<BiquadFilter> {
        val normalizedFreq = (2 * Math.PI * cutoffFreq / sampleRate).toFloat()
        val cosW0 = kotlin.math.cos(normalizedFreq)
        val sinW0 = kotlin.math.sin(normalizedFreq)
        val alpha = sinW0 / (2.0f * 0.707f) // Q = 0.707 for Butterworth

        val b0 = (1 - cosW0) / 2
        val b1 = 1 - cosW0
        val b2 = (1 - cosW0) / 2
        val a0 = 1 + alpha
        val a1 = -2 * cosW0
        val a2 = 1 - alpha

        return arrayOf(
            BiquadFilter(a0, a1, a2, b0, b1, b2),
            BiquadFilter(a0, a1, a2, b0, b1, b2) // Apply twice for 4th order
        )
    }
}
