#pragma once

#include "utils/JavaObject.h"
#include "DeviceInfo.h"

struct SpeakerTestResult {
    enum class MI {
        fillMicroInfo,
        fillSpeakerInfo,
        fillResult
    };

    SpeakerTestResult(SafeJavaVM &vm, jobject jcb) : vm(vm), o(vm, jcb) {
        MY_DBG();
    }

    void fillMicroInfo(const DeviceInfo &info) {
        MY_DBG();
        fillInfo<MI::fillMicroInfo>(info);
    }

    void fillSpeakerInfo(const DeviceInfo &info) {
        MY_DBG();
        fillInfo<MI::fillSpeakerInfo>(info);
    }

    void fillResult(uint32_t time, double form, double scale) {
        MY_DBG();
        o.call<MI::fillResult>(
                static_cast<jlong>(time),
                static_cast<jdouble>(form),
                static_cast<jdouble>(scale)
        );
    }

private:
    template<MI method>
    void fillInfo(const DeviceInfo &info) {
        MY_DBG();
        o.call<method>(
                vm.getEnv()->NewStringUTF(info.api),
                static_cast<jint>(info.id),
                static_cast<jint>(info.bytesPerSample),
                static_cast<jint>(info.bytesPerFrame),
                static_cast<jint>(info.framesPerBurst),
                static_cast<jint>(info.framesPerDataCallback),
                static_cast<jint>(info.sampleRate)
        );
    }

    SafeJavaVM &vm;

    JavaObject<MI,
            MethodDescription{
                    MI::fillMicroInfo,
                    ReturnType<void>{},
                    "fillMicroInfo",
                    "(Ljava/lang/String;IIIIII)V"
            },
            MethodDescription{
                    MI::fillSpeakerInfo,
                    ReturnType<void>{},
                    "fillSpeakerInfo",
                    "(Ljava/lang/String;IIIIII)V"
            },
            MethodDescription{
                    MI::fillResult,
                    ReturnType<void>{},
                    "fillResult",
                    "(JDD)V"
            }
    > o;
};