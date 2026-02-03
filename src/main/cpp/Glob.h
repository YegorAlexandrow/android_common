#pragma once

#include "utils/common.h"
#include "utils/SafeJavaVM.h"
#include "CallAec.h"
#include "SpeakerTest.h"
#include "SpeakerTestAlternative.h"
#include "Storage.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <memory>
using namespace std::literals;

template<class Options>
class Glob {

    using Cmd = typename DeviceDuplex<Options>::Cmd;
    using sample_t = typename Options::SampleT;
    using CallT = CallAec<Options>;
    using TestT = SpeakerTest<Options>;
//    using TestT = SpeakerTestAlternative<Options>;

private:
    std::unique_ptr<CallT> call;
    std::unique_ptr<TestT> test;
    std::unique_ptr<SpeakerTestCallback> cb;
    std::unique_ptr<Storage> storage;

    DeviceDuplex<Options> duplex;

public:

    SafeJavaVM vm;


    Glob(JavaVM *vm_) : vm(vm_) {
        MY_DBG();
        vm.passExtEnv();
    }

    auto &fromJni() {
        MY_DBG();
        vm.passExtEnv();
        return *this;
    }

    void bindStorage(jobject jstorage) {
        MY_DBG();
        storage = std::make_unique<Storage>(vm, jstorage);
    }

    void bindAudioBus(jobject jbus) {
        MY_DBG();
        call = std::make_unique<CallT>(duplex, vm, jbus, *storage);
        call->setOnFinish([&] {
            myLog(" native call finished");
            stopDuplex();
            closeDuplex();
        });
    }

    void bindSpeakerTestResources(JNIEnv *env, jobject jam, const char *name, jobject jcb) {

        MY_DBG();
        auto sound = SoundBuffer<sample_t>(loadSoundAsset(name, env, jam));
        test = std::make_unique<TestT>(duplex, vm, sound, *storage);

//        test = std::make_unique<TestT>(duplex, vm, *storage);

        cb = std::make_unique<SpeakerTestCallback>(vm, jcb);
        test->setOnResultCalculated(
                [&](uint32_t time, double form,
                    double scale) mutable {
                    (*cb)(duplex.spk_info, duplex.mic_info, time, form, scale);
                });

        test->setOnFinish([&]() {
            myLog(" native speaker test finished");
            stopDuplex();
            closeDuplex();
        });
    }

    void startCall() {
        openDuplex();
        startDuplex();
        call->start();
    }

    void stopCall() {
        call->stop();
    }

    auto &getBus() {
        return call->bus;
    }

    void runTest() {
        MY_DBG();
        openDuplex();
        startDuplex();
        test->start();
    }

private:
    void openDuplex() {
        duplex(Cmd::openSpeaker);
        duplex(Cmd::openMicro);
    }

    void closeDuplex() {
        duplex(Cmd::closeSpeaker);
        duplex(Cmd::closeMicro);
    }

    void startDuplex() {
        duplex(Cmd::startMicro);
        duplex(Cmd::startSpeaker);
    }

    void stopDuplex() {
        duplex(Cmd::stopMicro);
        duplex(Cmd::stopSpeaker);
    }

    static SoundBuffer<sample_t> loadSoundAsset(const char *name, JNIEnv *env, jobject jam) {
        if (jam) {
            AAssetManager *am = AAssetManager_fromJava(env, jam);
            if (am) {
                AAsset *assetFile = AAssetManager_open(am, name, AASSET_MODE_BUFFER);

                void *buf = const_cast<void *>(AAsset_getBuffer(assetFile));
                size_t size = AAsset_getLength(assetFile);
                myLog("%s:\n%zu", name, size);
                return SoundBuffer<sample_t>(buf, size);
            }
        }
        throw std::runtime_error("asset load failed");
    }

};