#pragma once

#include "utils/JavaObject.h"
#include "DeviceInfo.h"
#include "SpeakerTestResult.h"

struct SpeakerTestCallback {

    SpeakerTestCallback(SafeJavaVM &vm, jobject jcb) : vm(vm), o(vm, jcb) {
        MY_DBG();
    }

    void operator()(
            const DeviceInfo &spk_info,
            const DeviceInfo &mic_info,
            uint32_t time,
            double form,
            double scale
    ) {
        MY_DBG();
        auto res = SpeakerTestResult(vm, o.call<MI::getTestResultDestination>());
        res.fillSpeakerInfo(spk_info);
        res.fillMicroInfo(mic_info);
        res.fillResult(time, form, scale);
        o.call<MI::onTestResultReady>();
    }

private:
    enum class MI {
        getTestResultDestination,
        onTestResultReady
    };
    SafeJavaVM &vm;

    JavaObject<MI,
            MethodDescription{
                    MI::getTestResultDestination,
                    ReturnType<jobject>{},
                    "getTestResultDestination",
                    "()Ltech/fastsense/common/native_audio/NativeSpeakerTest$TestResult;"
            },
            MethodDescription{
                    MI::onTestResultReady,
                    ReturnType<void>{},
                    "onTestResultReady", "()V"
            }
    > o;
};