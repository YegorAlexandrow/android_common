#include "Glob.h"
#include "GccPhat.h"
#include "Fft.h"
#include <string>
#include <sstream>
#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "AecProcessor.h"
#include <oboe/Oboe.h>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "utils/temp.h"
#include "recorder.h"
#include "renderer.h"

static std::unique_ptr<Glob<DeviceOptions>> glob;
static gp::GccPhat *gcc_phat;
static gp::FFT_forward *forward_FFT = nullptr;

size_t captureLogCounter = 0;
sc::duration<double, std::milli> captureElapsedMax[7]{};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    MY_DBG();
    JNIEnv *env{};
    static std::once_flag global_flag;

    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        myLog<Prio::E>("vm->GetEnv failed");
        return -1;
    }

    std::call_once(global_flag, [&] {
        glob = std::make_unique<decltype(glob)::element_type>(vm);
    });

    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_setupTest(
        JNIEnv *env,
        jobject,
        jobject jam,
        jobject jcb,
        jstring jname) {
    MY_DBG();
    glob->fromJni().bindSpeakerTestResources(env, jam, env->GetStringUTFChars(jname, nullptr), jcb);
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_runAudioSystemTest(JNIEnv *, jobject) {
    MY_DBG();
    glob->fromJni().runTest();
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_startCall(JNIEnv *, jobject) {
    glob->fromJni().startCall();
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_stopCall(JNIEnv *, jobject) {
    glob->fromJni().stopCall();
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_setAudioBusRenderState(JNIEnv *, jobject,
                                                                           jboolean state) {
    glob->fromJni().getBus().setRenderState(state);
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_setAudioBusCaptureState(JNIEnv *, jobject,
                                                                            jboolean state) {
    glob->fromJni().getBus().setCaptureState(state);
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_setupCall(JNIEnv *, jobject, jobject jbus) {
    glob->fromJni().bindAudioBus(jbus);
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_setupStorage(JNIEnv *env, jobject thiz,
                                                                 jobject storage) {
    glob->fromJni().bindStorage(storage);
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_gccPhatInit(JNIEnv *env, jobject thiz,
                                                                jint size) {
    MY_DBG();

    gcc_phat = gp::GccPhat::create();
    gcc_phat->init(size);

}

extern "C"
JNIEXPORT jint JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_gccPhatExecute(JNIEnv *env, jobject thiz,
                                                                   jobject x, jobject y,
                                                                   jint margin) {
    MY_DBG();

    auto xData = static_cast<int16_t *>(env->GetDirectBufferAddress(x));
    auto yData = static_cast<int16_t *>(env->GetDirectBufferAddress(y));

    auto xCapacity = env->GetDirectBufferCapacity(x) / sizeof(int16_t);
    auto yCapacity = env->GetDirectBufferCapacity(y) / sizeof(int16_t);

    std::vector<int16_t> vecX(xData, xData + xCapacity);
    std::vector<int16_t> vecY(yData, yData + yCapacity);
//    auto sX = vectorToString(vecX);
//    auto sY = vectorToString(vecY);
//    myLog("cpp: %s", sX.c_str());
//    myLog("cpp: %s", sY.c_str());
    auto start = schrc::now();
    auto result = gcc_phat->execute(vecX, vecY, margin);
    sc::duration<double, std::milli> elapsed = schrc::now() - start;
    myLog("elapsed: %f", elapsed.count());
    return result;
}

extern "C" JNIEXPORT jlong JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_createAec(JNIEnv *env, jobject thiz,
                                                              jint reference_sample_rate,
                                                              jint input_sample_rate,
                                                              jint processing_sample_rate,
                                                              jint output_sample_rate) {
    MY_DBG();
    return reinterpret_cast<jlong>(new AECProcessor(reference_sample_rate, input_sample_rate,
                                                    processing_sample_rate, output_sample_rate));
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_destroyAec(JNIEnv *env, jobject thiz,
                                                               jlong handle) {
    MY_DBG();
    delete reinterpret_cast<AECProcessor *>(handle);
}



extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_aecProcessRender(JNIEnv *env, jobject thiz,
                                                                     jlong handle,
                                                                     jshortArray render_frame) {
    auto *render = static_cast<jshort *>(env->GetPrimitiveArrayCritical(render_frame, nullptr));
    auto processor = reinterpret_cast<AECProcessor *>(handle);
    processor->processRender(render);
    env->ReleasePrimitiveArrayCritical(render_frame, render, JNI_ABORT);
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_aecProcessCapture(JNIEnv *env, jobject thiz,
                                                                      jlong handle,
                                                                      jshortArray capture_frame,
                                                                      jfloat additional_gain) {
//    MY_DBG();
    auto ts0 = schrc::now();
    auto *processor = reinterpret_cast<AECProcessor *>(handle);
    auto ts1 = schrc::now();
    auto *capture = static_cast<jshort *>(env->GetPrimitiveArrayCritical(capture_frame, nullptr));
    std::vector<int16_t> output(OUT_FRAME_SAMPLES_HW);
    auto ts2 = schrc::now();
    processor->processCapture(capture, output.data());
    auto ts3 = schrc::now();
    if (additional_gain > 0) {
        float linear_gain = std::pow(10.0f, additional_gain / 20.0f);
        for (size_t i = 0; i < OUT_FRAME_SAMPLES_HW; i++) {
            float sample = static_cast<float>(output[i]) * linear_gain;
            output[i] = static_cast<int16_t>(
                    sample > INT16_MAX ? INT16_MAX :
                    (sample < INT16_MIN ? INT16_MIN : sample)
            );
        }
    }
    auto ts4 = schrc::now();
    env->ReleasePrimitiveArrayCritical(capture_frame, capture, JNI_ABORT);
    auto ts5 = schrc::now();
    auto result = env->NewShortArray(OUT_FRAME_SAMPLES_HW);
    env->SetShortArrayRegion(result, 0, OUT_FRAME_SAMPLES_HW,
                             reinterpret_cast<const jshort *>(output.data()));
    auto ts6 = schrc::now();
    sc::duration<double, std::milli> elapsed[7]{ts1 - ts0, ts2 - ts1, ts3 - ts2, ts4 - ts3,
                                                ts5 - ts4, ts6 - ts5, ts6 - ts0};
    for (int i = 0; i < 7; ++i)
        if (elapsed[i] > captureElapsedMax[i])
            captureElapsedMax[i] = elapsed[i];
    if (!(captureLogCounter++ % 2000))
        myLog("aecProcessCapture duration/max:\n1-0: %f/%f\n2-1: %f/%f\n3-2: %f/%f\n4-3: %f/%f\n5-4: %f/%f\n6-5: %f/%f\n6-0: %f/%f",
              elapsed[0].count(), captureElapsedMax[0].count(),
              elapsed[1].count(), captureElapsedMax[1].count(),
              elapsed[2].count(), captureElapsedMax[2].count(),
              elapsed[3].count(), captureElapsedMax[3].count(),
              elapsed[4].count(), captureElapsedMax[4].count(),
              elapsed[5].count(), captureElapsedMax[5].count(),
              elapsed[6].count(), captureElapsedMax[6].count()
        );

    return result;
}

extern "C"
JNIEXPORT jint JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_aecGetDelay(JNIEnv *env, jobject thiz,
                                                                jlong handle) {
    return reinterpret_cast<AECProcessor *>(handle)->getDelay();
}

extern "C" JNIEXPORT jlong JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeCreateRecorder(
        JNIEnv *env,
        jobject /* this */,
        jint sampleRate,
        jint framesPerBuffer,
        jint id) {
    MY_DBG();
    return reinterpret_cast<jlong>(new BlockingRecorder(glob->vm, sampleRate, framesPerBuffer, id));
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeDestroyRecorder(
        JNIEnv *env,
        jobject /* this */,
        jlong handle) {
    MY_DBG();
    auto recorder = reinterpret_cast<BlockingRecorder *>(handle);
    delete recorder;
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeStartRecording(
        JNIEnv *env,
        jobject /* this */,
        jlong handle) {
    MY_DBG();
    auto recorder = reinterpret_cast<BlockingRecorder *>(handle);
    recorder->start();
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeStopRecording(
        JNIEnv *env,
        jobject /* this */,
        jlong handle) {
    MY_DBG();
    auto recorder = reinterpret_cast<BlockingRecorder *>(handle);
    recorder->stop();
}


// JNI Functions implementation
extern "C" JNIEXPORT jlong JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeCreateRenderer(JNIEnv *env, jobject thiz,
                                                                       jint sample_rate,
                                                                       jint frames_per_buffer,
                                                                       jfloat attenuationFactor,
                                                                       jfloat smoothing,
                                                                       jfloat downSmoothing,
                                                                       jfloat threshold,
                                                                       jint downStartSamples) {
    MY_DBG();
    auto renderer = new BlockingAudioRenderer(glob->vm, sample_rate, frames_per_buffer,
                                              attenuationFactor, smoothing, downSmoothing,
                                              threshold, downStartSamples);
    if (!renderer->initialize()) {
        delete renderer;
        return 0;
    }
    return reinterpret_cast<jlong>(renderer);
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeDestroyRenderer(JNIEnv *env, jobject thiz,
                                                                        jlong handle) {
    MY_DBG();
    delete reinterpret_cast<BlockingAudioRenderer *>(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeStartRendering(JNIEnv *env, jobject thiz,
                                                                       jlong handle) {
    MY_DBG();
    auto renderer = reinterpret_cast<BlockingAudioRenderer *>(handle);
    if (renderer) renderer->start();
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeStopRendering(JNIEnv *env, jobject thiz,
                                                                      jlong handle) {
    MY_DBG();
    auto renderer = reinterpret_cast<BlockingAudioRenderer *>(handle);
    if (renderer) renderer->stop();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeIsPlaying(JNIEnv *env, jobject thiz,
                                                                  jlong handle) {
    MY_DBG();
    auto renderer = reinterpret_cast<BlockingAudioRenderer *>(handle);
    return renderer && renderer->isPlaying() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_initSpectro(JNIEnv *env, jobject thiz, jint n) {
    forward_FFT = gp::FFT_forward::create();
    forward_FFT->init(n);
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_runSpectro(JNIEnv *env, jobject thiz,
                                                               jshortArray shorts,
                                                               jfloatArray magnitudes,
                                                               jdouble db_range,
                                                               jdouble db_gain) {
    jsize input_len = env->GetArrayLength(shorts);
    jshort *input_data = env->GetShortArrayElements(shorts, nullptr);
    jfloat *output_data = env->GetFloatArrayElements(magnitudes, nullptr);

    std::vector<int16_t> input_vec(input_data, input_data + input_len);
    std::vector<std::complex<double>> fft_output;

    forward_FFT->execute(fft_output, input_vec);

    const double normalization_factor = 1.0 / (32768.0 * fft_output.size());

    const double min_dB = db_range;
    const double max_dB = 0.0 + db_gain;
    const double reference_level = 1.0;

    for (int i = 0; i < fft_output.size(); i++) {
        const auto &complex_val = fft_output[i];
        double magnitude = std::sqrt(complex_val.real() * complex_val.real() +
                                     complex_val.imag() * complex_val.imag());

        // Apply normalization
        double normalized_magnitude = magnitude * normalization_factor;

        // Convert to dB
        double magnitude_dB = 20.0 * std::log10(normalized_magnitude / reference_level + 1e-10);

        magnitude_dB += db_gain;

        magnitude_dB = std::max(min_dB, std::min(max_dB, magnitude_dB));
        output_data[i] = (float) ((magnitude_dB - min_dB) / (max_dB - min_dB));
    }

    // Release arrays
    env->ReleaseShortArrayElements(shorts, input_data, JNI_ABORT);
    env->ReleaseFloatArrayElements(magnitudes, output_data, 0);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_pushRenderBuffer(JNIEnv *env, jobject thiz,
                                                                     jlong handle) {
    return BlockingAudioRenderer::fromHandle(handle)->pushRenderBuffer();
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_popReferenceBuffer(JNIEnv *env, jobject thiz,
                                                                       jlong handle) {
    return BlockingAudioRenderer::fromHandle(handle)->popReferenceBuffer();
}
extern "C"
JNIEXPORT jobject JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_getRenderBuffer(JNIEnv *env, jobject thiz,
                                                                    jlong handle) {
    return BlockingAudioRenderer::fromHandle(handle)->getRenderBuffer();
}
extern "C"
JNIEXPORT jobject JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_getReferenceBuffer(JNIEnv *env, jobject thiz,
                                                                       jlong handle) {
    return BlockingAudioRenderer::fromHandle(handle)->getReferenceBuffer();
}
extern "C"
JNIEXPORT jobject JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_getCaptureBuffer(JNIEnv *env, jobject thiz,
                                                                     jlong handle) {
    return BlockingRecorder::fromHandle(handle)->getCaptureBuffer();

}
extern "C"
JNIEXPORT jboolean JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_blockingPopCaptureBuffer(JNIEnv *env,
                                                                             jobject thiz,
                                                                             jlong handle) {
    return BlockingRecorder::fromHandle(handle)->blockingPopCaptureBuffer();
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_updateVad(JNIEnv *env, jobject thiz,
                                                              jlong handle, jboolean vad,
                                                              jfloat conf) {
    BlockingAudioRenderer::fromHandle(handle)->updateVad(vad, conf);
}


extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_updateMuted(JNIEnv *env, jobject thiz,
                                                                jlong handle, jboolean muted) {
    BlockingAudioRenderer::fromHandle(handle)->updateMuted(muted);
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_handleDisconnect(JNIEnv *env, jobject thiz,
                                                                     jlong handle) {
    BlockingAudioRenderer::fromHandle(handle)->handleDisconnect();
}
