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

std::string vectorToString(const std::vector<int16_t> &vec) {
    std::stringstream ss;
    for (size_t i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i < vec.size() - 1) {
            ss << " ";  // Separate each number with a space
        }
    }
    return ss.str();
}

static std::unique_ptr<Glob<DeviceOptions>> glob;
static gp::GccPhat *gcc_phat;

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
    namespace sc = std::chrono;
    using schrc = sc::high_resolution_clock;
    auto start = schrc::now();
    auto result = gcc_phat->execute(vecX, vecY, margin);
    sc::duration<double, std::milli> elapsed = schrc::now() - start;
    myLog("elapsed: %f", elapsed.count());
    return result;
}


// Load PCM16 data from a file
std::vector<int16_t> LoadPCM16File(const std::string &filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<int16_t> buffer(size / sizeof(int16_t));
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + filepath);
    }

    return buffer;
}

// Save PCM16 data to a file
void SavePCM16File(const std::string &filepath, const std::vector<int16_t> &data) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    if (!file.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(int16_t))) {
        throw std::runtime_error("Failed to write file: " + filepath);
    }
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

namespace sc = std::chrono;
using schrc = sc::high_resolution_clock;

size_t captureLogCounter = 0;
sc::duration<double, std::milli> captureElapsedMax[7]{};

extern "C" JNIEXPORT jshortArray JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_aecProcessCapture(JNIEnv *env, jobject thiz,
                                                                      jlong handle,
                                                                      jshortArray capture_frame,
                                                                      jfloat additional_gain) {
//    MY_DBG();
    auto ts0 = schrc::now();
    auto *processor = reinterpret_cast<AECProcessor *>(handle);
    auto output_frame_size = processor->output_frame_size_;
    auto ts1 = schrc::now();
    auto *capture = static_cast<jshort *>(env->GetPrimitiveArrayCritical(capture_frame, nullptr));
    std::vector<int16_t> output(output_frame_size);
    auto ts2 = schrc::now();
    processor->processCapture(capture, output.data());
    auto ts3 = schrc::now();
    if (additional_gain > 0) {
        float linear_gain = std::pow(10.0f, additional_gain / 20.0f);
        for (int i = 0; i < output_frame_size; i++) {
            float sample = static_cast<float>(output[i]) * linear_gain;
            output[i] = static_cast<int16_t>(
                    sample > INT16_MAX ? INT16_MAX :
                    (sample < INT16_MIN ? INT16_MIN : sample)
            );
        }
    }
    auto ts4 = schrc::now();
    auto result = env->NewShortArray(output_frame_size);
    env->SetShortArrayRegion(result, 0, output_frame_size,
                             reinterpret_cast<const jshort *>(output.data()));
    auto ts5 = schrc::now();
    env->ReleasePrimitiveArrayCritical(capture_frame, capture, JNI_ABORT);
    auto ts6 = schrc::now();
    sc::duration<double, std::milli> elapsed[7]{ts1 - ts0, ts2 - ts1, ts3 - ts2, ts4 - ts3,
                                                ts5 - ts4, ts6 - ts5, ts6 - ts0};
    for (int i = 0; i < 7; ++i)
        if (elapsed[i] > captureElapsedMax[i])
            captureElapsedMax[i] = elapsed[i];
    if (!(captureLogCounter++ % 1000))
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

#define LOG_TAG "OboeBlockingRecorder"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

class BlockingRecorder : public AudioStreamCallback {
public:
    BlockingRecorder(SafeJavaVM &vm_, int sampleRate, int framesPerBuffer, int id)
            : vm(vm_), mSampleRate(sampleRate), mFramesPerBuffer(framesPerBuffer) {
        createStream(id);
        auto env = vm.getEnv();
        result = (jbyteArray) env->NewGlobalRef(env->NewByteArray(mFramesPerBuffer * 2));
    }

    ~BlockingRecorder() {
        closeStream();
    }

    void start(jobject jcb) {
        if (mStream) {
            cb = std::make_unique<CaptureCallback>(vm, jcb);
            mStream->requestStart();
        }
    }

    void stop() {
        if (mStream) {
            mStream->stop();
        }
    }

    size_t logCounter = 0;


private:
    void createStream(int id) {
        oboe::AudioStreamBuilder builder;

        builder.setAudioApi(AudioApi::AAudio)
                ->setUsage(oboe::Usage::Game)
                ->setInputPreset(oboe::InputPreset::Unprocessed)
                ->setDeviceId(id)
                ->setDirection(oboe::Direction::Input)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Exclusive)
                ->setFormat(oboe::AudioFormat::I16)
                ->setChannelCount(1)
                ->setBufferCapacityInFrames(mFramesPerBuffer * 2)
                ->setSampleRate(mSampleRate)
                ->setFramesPerDataCallback(mFramesPerBuffer)
                ->setCallback(this);

        oboe::Result result = builder.openStream(mStream);
        if (result != oboe::Result::OK) {
            LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
            mError = true;
        }
    }

    void closeStream() {
        if (mStream) {
            mStream->close();
            mStream.reset();
        }
    }

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *stream,
            void *audioData,
            int32_t numFrames) override {

        auto env = vm.getEnv();
        env->SetByteArrayRegion(result, 0, mFramesPerBuffer * 2, (jbyte *) audioData);
        cb->run(result);

        if (!(logCounter++ % 1000)) {
            myLog("MIC: FramesPerBurst: %d, XRunCount: %d, "
                  "BufferCapacityInFrames: %d, BufferSizeInFrames: %d",
                  mStream->getFramesPerBurst(),
                  mStream->getXRunCount().value(),
                  mStream->getBufferCapacityInFrames(), mStream->getBufferSizeInFrames()
            );
        }

        return oboe::DataCallbackResult::Continue;
    }

    struct CaptureCallback {

        CaptureCallback(SafeJavaVM &vm, jobject jcb) : o(vm, jcb) {
            MY_DBG();
        }

        void run(jbyteArray data) {
            MY_DBG();
            return o.call<MI::run>(data);
        }

    private:
        enum class MI {
            run
        };

        JavaObject<MI,
                MethodDescription{
                        MI::run,
                        ReturnType<void>{},
                        "run",
                        "([B)V"
                }> o;
    };


    SafeJavaVM &vm;

    int mSampleRate;
    int mFramesPerBuffer;
    jbyteArray result;
    std::shared_ptr<oboe::AudioStream> mStream;
    std::unique_ptr<CaptureCallback> cb;
    bool mError = false;
};

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
        jlong handle, jobject jcb) {
    MY_DBG();
    auto recorder = reinterpret_cast<BlockingRecorder *>(handle);
    recorder->start(jcb);
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

class BlockingAudioRenderer : public oboe::AudioStreamCallback {
public:
    BlockingAudioRenderer(SafeJavaVM &vm_, int32_t sampleRate, int32_t framesPerBuffer)
            : vm(vm_), sampleRate_(sampleRate), framesPerBuffer_(framesPerBuffer),
              renderMirrorBuffer(vm_), referenceMirrorBuffer(vm_) {}

    ~BlockingAudioRenderer() {
        stop();
        closeStream();
    }

    bool initialize() {
        oboe::AudioStreamBuilder builder;
        builder.setAudioApi(AudioApi::AAudio)
                ->setUsage(oboe::Usage::Game)
                ->setDirection(oboe::Direction::Output)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Exclusive)
                ->setFormat(oboe::AudioFormat::I16)
                ->setChannelCount(1)
                ->setBufferCapacityInFrames(framesPerBuffer_ * 2)
                ->setSampleRate(sampleRate_)
                ->setFramesPerDataCallback(framesPerBuffer_)
                ->setCallback(this);

        auto result = builder.openStream(stream_);
        return result == oboe::Result::OK && stream_ != nullptr;
    }

    void start(jobject jcb, jlong handle) {
        if (stream_) {
            aecHandle = handle;
            cb = std::make_unique<RenderCallback>(vm, jcb);
            stream_->requestStart();
        }
    }

    void stop() {
        if (stream_) {
            stream_->requestStop();
        }
    }

    void closeStream() {
        if (stream_) {
            stream_->close();
            stream_.reset();
        }
    }

    bool pushRenderBuffer() {
//        if (!stream_) return; we need to have ability to pre-feed render with eg aec warmup frames
        myLog("pushRenderBuffer %d", renderMirrorBuffer.data->meta.traceId);

        return renderQueue.push(*renderMirrorBuffer.data);
    }

    bool popReferenceBuffer() {
        return referenceQueue.pop(*referenceMirrorBuffer.data);
    }

    jobject getRenderBuffer() { return *renderMirrorBuffer.o; }

    jobject getReferenceBuffer() { return *referenceMirrorBuffer.o; }

    bool isPlaying() const {
        return stream_ && stream_->getState() == oboe::StreamState::Started;
    }

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) override {
        const auto ts0 = schrc::now();
        auto renderData = silentAudioRenderData;
        auto output = static_cast<decltype(renderData.frame) *>(audioData);
        const auto ts1 = schrc::now();
        const auto underrunCount = stream_->getXRunCount().value();
        const auto ts2 = schrc::now();
        while (underrunCount - underrunCountHandled > 0) {
            referenceQueue.push(underrunAudioRenderData);
            underrunCountHandled++;
        }
        const auto ts3 = schrc::now();

        [[maybe_unused]] const auto res = renderQueue.dropWhilePop(
                renderData, [&](const AudioRenderDataHw &data) {
                    return data.meta.traceId <= traceIdToDrop.load();
                });
        const auto ts4 = schrc::now();
        *output = renderData.frame;
        const auto ts5 = schrc::now();
        referenceQueue.push(renderData);
        const auto ts6 = schrc::now();
        sc::duration<double, std::milli> elapsed[7]{ts1 - ts0, ts2 - ts1, ts3 - ts2, ts4 - ts3,
                                                    ts5 - ts4, ts6 - ts5, ts6 - ts0};

        for (int i = 0; i < 7; ++i)
            if (elapsed[i] > renderElapsedMax[i])
                renderElapsedMax[i] = elapsed[i];

        if (logCounter++ % 1000 == 0) {
            myLog("XRunCount: %d\nduration/max:\n1-0: %f/%f\n2-1: %f/%f\n3-2: %f/%f\n4-3: %f/%f\n5-4: %f/%f\n6-5: %f/%f\n6-0: %f/%f",
                  underrunCount,
                  elapsed[0].count(), renderElapsedMax[0].count(),
                  elapsed[1].count(), renderElapsedMax[1].count(),
                  elapsed[2].count(), renderElapsedMax[2].count(),
                  elapsed[3].count(), renderElapsedMax[3].count(),
                  elapsed[4].count(), renderElapsedMax[4].count(),
                  elapsed[5].count(), renderElapsedMax[5].count(),
                  elapsed[6].count(), renderElapsedMax[6].count()
            );
        }
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) override {
        if (error == oboe::Result::ErrorDisconnected) {
            closeStream();
            initialize();
            if (stream_) {
                stream_->requestStart();
            }
        }
    }

    static BlockingAudioRenderer *fromHandle(jlong handle) {
        auto renderer = reinterpret_cast<BlockingAudioRenderer *>(handle);
        if (!renderer) throw std::runtime_error("Cannot get audio renderer from handle");
        return renderer;
    }

private:
    void sendAecReference(int16_t *frame) const {
        auto processor = reinterpret_cast<AECProcessor *>(aecHandle);
        processor->processRender(frame);
    }

    struct RenderCallback {

        RenderCallback(SafeJavaVM &vm, jobject jcb) : vm(vm), o(vm, jcb) {
            MY_DBG();
        }

        jshortArray run(int underrunCount) {
            MY_DBG();
            return reinterpret_cast<jshortArray>(o.call<MI::run>(underrunCount));
        }

    private:
        enum class MI {
            run
        };
        SafeJavaVM &vm;

        JavaObject<MI,
                MethodDescription{
                        MI::run,
                        ReturnType<jobject>{},
                        "run",
                        "(I)[S"
                }> o;
    };

    SafeJavaVM &vm;
    jlong aecHandle;
    int32_t underrunCountHandled = 0;
    sc::duration<double, std::milli> renderElapsedMax[7]{};

    std::unique_ptr<RenderCallback> cb;
    std::shared_ptr<oboe::AudioStream> stream_;

    int32_t sampleRate_;
    int32_t framesPerBuffer_;

    size_t logCounter = 0;

    AudioRenderDataMirrorBufferHw renderMirrorBuffer;
    AtomicQueue<AudioRenderDataHw, 8192> renderQueue;

    AudioRenderDataMirrorBufferHw referenceMirrorBuffer;
    AtomicQueue<AudioRenderDataHw, 256> referenceQueue;

    std::atomic<int32_t> traceIdToDrop{INIT_ID};
};

// JNI Functions implementation
extern "C" JNIEXPORT jlong JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeCreateRenderer(JNIEnv *env, jobject thiz,
                                                                       jint sample_rate,
                                                                       jint frames_per_buffer) {
    MY_DBG();
    auto renderer = new BlockingAudioRenderer(glob->vm, sample_rate, frames_per_buffer);
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
                                                                       jlong handle,
                                                                       jlong aecHandle,
                                                                       jobject jcb) {
    MY_DBG();
    auto renderer = reinterpret_cast<BlockingAudioRenderer *>(handle);
    if (renderer) renderer->start(jcb, aecHandle);
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

static gp::FFT_forward *forward_FFT = nullptr;

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