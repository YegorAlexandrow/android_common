#include "Glob.h"
#include "GccPhat.h"
#include <string>
#include <sstream>
#include <cstdint>
#include <iostream>

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
