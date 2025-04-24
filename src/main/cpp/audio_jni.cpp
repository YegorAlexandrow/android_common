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
Java_tech_fastsense_common_native_1audio_JniWrapper_createAec(JNIEnv *env, jobject thiz) {
    MY_DBG();
    return reinterpret_cast<jlong>(new AECProcessor());
}

extern "C" JNIEXPORT void JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_destroyAec(JNIEnv *env, jobject thiz,
                                                               jlong handle) {
    MY_DBG();
    delete reinterpret_cast<AECProcessor *>(handle);
}

// current AECProcessor impl can work only with 240sa 24khz render and 160sa 16khz capture/result
constexpr auto samples_per_frame = 160;
constexpr auto samples_per_render_frame = samples_per_frame * 24 / 16;
//constexpr auto samples_per_render_frame = samples_per_frame;

bool validateChunks(JNIEnv *env, jshortArray render_frame, jshortArray capture_frame) {
    const auto renderFrameSize = env->GetArrayLength(render_frame);
    const auto captureFrameSize = env->GetArrayLength(capture_frame);

    const auto passed = renderFrameSize == samples_per_render_frame &&
                        captureFrameSize == samples_per_frame;
    if (!passed)
        myLog<Prio::E>("wrong frame sizes (r|c): %zu | %zu. requested: %zu | %zu",
                       renderFrameSize, captureFrameSize,
                       samples_per_render_frame, samples_per_frame);
    return passed;
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_tech_fastsense_common_native_1audio_JniWrapper_aecProcessFrame(
        JNIEnv *env, jobject thiz, jlong handle,
        jshortArray render_frame, jshortArray capture_frame, jfloat additional_gain) {
//    MY_DBG();

    if (handle == 0 || render_frame == nullptr || capture_frame == nullptr) return nullptr;
    if (!validateChunks(env, render_frame, capture_frame)) return nullptr;

    auto *processor = reinterpret_cast<AECProcessor *>(handle);

    jshort *render = env->GetShortArrayElements(render_frame, nullptr);
    jshort *capture = env->GetShortArrayElements(capture_frame, nullptr);
    std::vector<int16_t> output(samples_per_frame);

    processor->processFrame(render, capture, output.data());

    if (additional_gain > 0) {
        float linear_gain = std::pow(10.0f, additional_gain / 20.0f);
        for (int i = 0; i < samples_per_frame; i++) {
            float sample = static_cast<float>(output[i]) * linear_gain;
            output[i] = static_cast<int16_t>(
                    sample > INT16_MAX ? INT16_MAX :
                    (sample < INT16_MIN ? INT16_MIN : sample)
            );
        }
    }

    jshortArray result = env->NewShortArray(samples_per_frame);
    env->SetShortArrayRegion(result, 0, samples_per_frame,
                             reinterpret_cast<const jshort *>(output.data()));

    env->ReleaseShortArrayElements(render_frame, render, JNI_ABORT);
    env->ReleaseShortArrayElements(capture_frame, capture, JNI_ABORT);
    return result;
}

class RecordingCallback : public oboe::AudioStreamCallback {
public:
    RecordingCallback(std::ofstream &outFile) : mOutFile(outFile) {}

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) override {
        mOutFile.write(static_cast<const char*>(audioData), numFrames * sizeof(int16_t));
        return oboe::DataCallbackResult::Continue;
    }

private:
    std::ofstream &mOutFile;
};
#define LOG_TAG "OboeRecording"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
        Java_tech_fastsense_common_native_1audio_JniWrapper_recordFromMicrophone(
                JNIEnv *env,
                jobject /* this */,
                jint deviceId,
                jstring jFilePath) {

    const char *filePath = env->GetStringUTFChars(jFilePath, nullptr);
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        LOGE("Failed to open output file");
        env->ReleaseStringUTFChars(jFilePath, filePath);
        return;
    }

    oboe::AudioStreamBuilder builder;
    builder.setDeviceId(deviceId)
            ->setDirection(oboe::Direction::Input)
            ->setInputPreset(oboe::InputPreset::Unprocessed)
            ->setPrivacySensitiveMode(oboe::PrivacySensitiveMode::Enabled);
//            ->setSampleRate(16000)
//            ->setChannelCount(oboe::ChannelCount::Mono)
//            ->setFormat(oboe::AudioFormat::I16)
//            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
//            ->setInputPreset(oboe::InputPreset::VoicePerformance);

    RecordingCallback callback(outFile);
    builder.setCallback(&callback);

    std::shared_ptr<oboe::AudioStream> stream;
    oboe::Result result = builder.openStream(stream);

    if (result != oboe::Result::OK) {
        LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
        outFile.close();
        env->ReleaseStringUTFChars(jFilePath, filePath);
        return;
    }

    result = stream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start stream. Error: %s", oboe::convertToText(result));
        outFile.close();
        env->ReleaseStringUTFChars(jFilePath, filePath);
        return;
    }

    LOGI("Recording started for 10 seconds...");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    stream->stop();
    stream->close();
    outFile.close();
    env->ReleaseStringUTFChars(jFilePath, filePath);
    LOGI("Recording completed and saved to %s", filePath);
}