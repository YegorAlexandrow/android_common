#pragma once

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

class BlockingRecorder : public AudioStreamCallback {
public:
    BlockingRecorder(SafeJavaVM &vm_, int sampleRate, int framesPerBuffer, int id)
            : mSampleRate(sampleRate), mFramesPerBuffer(framesPerBuffer),
              captureMirrorBuffer(vm_) {
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

    bool blockingPopCaptureBuffer() {
        return captureQueue.blockingPop(*captureMirrorBuffer.data);
    }

    jobject getCaptureBuffer() { return *captureMirrorBuffer.o; }

    size_t logCounter = 0;

    static BlockingRecorder *fromHandle(jlong handle) {
        auto recorder = reinterpret_cast<BlockingRecorder *>(handle);
        if (!recorder) throw std::runtime_error("Cannot get audio recorder from handle");
        return recorder;
    }

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
            myLog<Prio::E>("Failed to create stream. Error: %s", oboe::convertToText(result));
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

        if (!(logCounter++ % 2000)) {
            myLog("MIC: FramesPerBurst: %d, XRunCount: %d, "
                  "BufferCapacityInFrames: %d, BufferSizeInFrames: %d",
                  mStream->getFramesPerBurst(),
                  mStream->getXRunCount().value(),
                  mStream->getBufferCapacityInFrames(), mStream->getBufferSizeInFrames()
            );
        }
        captureQueue.pushNotify(*static_cast<AudioCaptureDataHw *>(audioData));

        return oboe::DataCallbackResult::Continue;
    }

    int mSampleRate;
    int mFramesPerBuffer;
    std::shared_ptr<oboe::AudioStream> mStream;
    bool mError = false;

    AudioCaptureDataMirrorBufferHw captureMirrorBuffer;
    AtomicQueue<AudioCaptureDataHw, 256> captureQueue;
};
