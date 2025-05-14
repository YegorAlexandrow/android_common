#include "Glob.h"
#include "GccPhat.h"
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

    auto xData = static_cast<int16_t*>(env->GetDirectBufferAddress(x));
    auto yData = static_cast<int16_t*>(env->GetDirectBufferAddress(y));

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

extern "C" JNIEXPORT jshortArray JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_aecProcessFrame(
        JNIEnv *env, jobject thiz, jlong handle,
        jshortArray render_frame, jshortArray capture_frame, jfloat additional_gain) {
//    MY_DBG();

    if (handle == 0 || render_frame == nullptr || capture_frame == nullptr) return nullptr;

    auto *processor = reinterpret_cast<AECProcessor *>(handle);
    if (!processor->validateChunks(env, render_frame, capture_frame)) return nullptr;
    auto output_frame_size = processor->output_frame_size_;

    jshort *render = env->GetShortArrayElements(render_frame, nullptr);
    jshort *capture = env->GetShortArrayElements(capture_frame, nullptr);
    std::vector<int16_t> output(output_frame_size);

    processor->processFrame(render, capture, output.data());

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

    jshortArray result = env->NewShortArray(output_frame_size);
    env->SetShortArrayRegion(result, 0, output_frame_size,
                             reinterpret_cast<const jshort *>(output.data()));

    env->ReleaseShortArrayElements(render_frame, render, JNI_ABORT);
    env->ReleaseShortArrayElements(capture_frame, capture, JNI_ABORT);
    return result;
}

#define LOG_TAG "OboeBlockingRecorder"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

class BlockingRecorder : public AudioStreamCallback {
public:
    BlockingRecorder(int sampleRate, int framesPerBuffer, int id)
            : mSampleRate(sampleRate), mFramesPerBuffer(framesPerBuffer) {
        mAudioBuffer.resize(framesPerBuffer * 4);
        createStream(id);
    }

    ~BlockingRecorder() {
        closeStream();
    }

    void start() {
        if (mStream) {
            mStream->requestStart();
        }
    }

    void stop() {
        if (mStream) {
            mStream->stop();
        }
    }

    void read(void *buffer) {
        std::unique_lock<std::mutex> lock(mMutex);

        mCondition.wait(lock, [this] {
            return ((mAvailableFrames >= mFramesPerBuffer) || mError);
        });

        if (mError) {
            memset(buffer, 0, mFramesPerBuffer * sizeof(int16_t));
            return;
        }

        size_t firstPart = std::min((size_t) mFramesPerBuffer, mAudioBuffer.size() - mReadPos);
        memcpy(buffer, mAudioBuffer.data() + mReadPos, firstPart * sizeof(int16_t));

        if (firstPart < (size_t) mFramesPerBuffer) {
            memcpy((int16_t *) buffer + firstPart, mAudioBuffer.data(),
                   (mFramesPerBuffer - firstPart) * sizeof(int16_t));
        }

        mReadPos = (mReadPos + mFramesPerBuffer) % mAudioBuffer.size();
        mAvailableFrames -= mFramesPerBuffer;
    }

private:
    void createStream(int id) {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Input)
                ->setDeviceId(id)
                ->setInputPreset(oboe::InputPreset::Unprocessed)
                ->setSampleRate(mSampleRate)
                ->setChannelCount(oboe::ChannelCount::Mono)
                ->setFormat(oboe::AudioFormat::I16)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setFramesPerCallback(mFramesPerBuffer)
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
        std::lock_guard<std::mutex> lock(mMutex);

        if ((mAudioBuffer.size() - mAvailableFrames) < (size_t) numFrames) {
            LOGE("Buffer overflow - increasing buffer size");
            mAudioBuffer.resize(mAudioBuffer.size() * 2);
        }

        size_t writePos = (mReadPos + mAvailableFrames) % mAudioBuffer.size();
        size_t firstPart = std::min((size_t) numFrames, mAudioBuffer.size() - writePos);

        memcpy(mAudioBuffer.data() + writePos, audioData, firstPart * sizeof(int16_t));
        if (firstPart < (size_t) numFrames) {
            memcpy(mAudioBuffer.data(), (int16_t *) audioData + firstPart,
                   (numFrames - firstPart) * sizeof(int16_t));
        }

        mAvailableFrames += numFrames;
        mCondition.notify_one();
        return oboe::DataCallbackResult::Continue;
    }

    int mSampleRate;
    int mFramesPerBuffer;
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<int16_t> mAudioBuffer;
    std::mutex mMutex;
    std::condition_variable mCondition;
    size_t mReadPos = 0;
    size_t mAvailableFrames = 0;
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
    return reinterpret_cast<jlong>(new BlockingRecorder(sampleRate, framesPerBuffer, id));
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

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_oboeRead(
        JNIEnv *env,
        jobject /* this */,
        jlong handle,
        jbyteArray buffer) {
//    MY_DBG();
    auto recorder = reinterpret_cast<BlockingRecorder *>(handle);
    jbyte *bufferPtr = env->GetByteArrayElements(buffer, nullptr);
    recorder->read(bufferPtr);
    env->ReleaseByteArrayElements(buffer, bufferPtr, 0);
}