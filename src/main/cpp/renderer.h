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

class BlockingAudioRenderer : public oboe::AudioStreamCallback {
public:
    BlockingAudioRenderer(SafeJavaVM &vm_, int32_t sampleRate, int32_t framesPerBuffer,
                          jfloat attenuationFactor,
                          jfloat smoothing, jfloat downSmoothing, jfloat threshold,
                          jint downStartSamples)
            : attenuationFactor(attenuationFactor), smoothing(smoothing),
              downSmoothing(downSmoothing), threshold(threshold),
              downStartSamples(downStartSamples),
              sampleRate_(sampleRate), framesPerBuffer_(framesPerBuffer),
              renderMirrorBuffer(vm_), referenceMirrorBuffer(vm_) {
        myLog("attenuationFactor: %f\nsmoothing: %f\ndownSmoothing: %f\nthreshold: %f\ndownStartSamples: %d",
              attenuationFactor, smoothing, downSmoothing, threshold, downStartSamples);
    }

    ~BlockingAudioRenderer() {
        stop();
        closeStream();
    }

    bool initialize() {
        // Output stream creation is fragile across devices (exclusive/mono/low-latency may fail).
        // Try a few fallbacks and log errors so Android-side can understand why the renderer handle is 0.
        struct Attempt {
            const char *label;
            oboe::SharingMode sharing;
            oboe::PerformanceMode perf;
            int channels;
        };

        const Attempt attempts[] = {
                {"AAudio exclusive LL mono", oboe::SharingMode::Exclusive, oboe::PerformanceMode::LowLatency, 1},
                {"AAudio exclusive LL stereo", oboe::SharingMode::Exclusive, oboe::PerformanceMode::LowLatency, 2},
                {"AAudio shared LL mono", oboe::SharingMode::Shared, oboe::PerformanceMode::LowLatency, 1},
                {"AAudio shared LL stereo", oboe::SharingMode::Shared, oboe::PerformanceMode::LowLatency, 2},
                {"AAudio shared none mono", oboe::SharingMode::Shared, oboe::PerformanceMode::None, 1},
                {"AAudio shared none stereo", oboe::SharingMode::Shared, oboe::PerformanceMode::None, 2},
        };

        for (const auto &a: attempts) {
            oboe::AudioStreamBuilder builder;
            builder.setAudioApi(AudioApi::AAudio)
                    ->setUsage(oboe::Usage::Game)
                    ->setDirection(oboe::Direction::Output)
                    ->setPerformanceMode(a.perf)
                    ->setSharingMode(a.sharing)
                    ->setFormat(oboe::AudioFormat::I16)
                    ->setChannelCount(a.channels)
                    ->setBufferCapacityInFrames(framesPerBuffer_ * 2)
                    ->setSampleRate(sampleRate_)
                    ->setFramesPerDataCallback(framesPerBuffer_)
                    ->setCallback(this);

            auto result = builder.openStream(stream_);
            if (result == oboe::Result::OK && stream_ != nullptr) {
                myLog("Renderer stream opened (%s): api=%s ch=%d sr=%d fpb=%d",
                      a.label,
                      oboe::convertToText(stream_->getAudioApi()),
                      stream_->getChannelCount(),
                      stream_->getSampleRate(),
                      stream_->getFramesPerBurst());
                return true;
            }
            myLog<Prio::E>("Renderer openStream failed (%s): %s", a.label, oboe::convertToText(result));
            stream_.reset();
        }
        return false;
    }

    void start() {
        if (stream_) {
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
//        if (!stream_) return; we need to have ability to pre-feed render with file frames
        return renderQueue.push(*renderMirrorBuffer.data);
    }

    bool popReferenceBuffer() {
        return referenceQueue.pop(*referenceMirrorBuffer.data);
    }

    jobject getRenderBuffer() { return *renderMirrorBuffer.o; }

    jobject getReferenceBuffer() { return *referenceMirrorBuffer.o; }

    void updateVad(bool _vad, float conf) {
        vad = _vad;
        vadConf = conf;
    }

    void updateMuted(bool _muted) {
        muted = _muted;
    }

    void handleDisconnect() {
        traceIdToDrop = INIT_ID;
        latestAvatarTraceId = INIT_ID;
        latestPlayedTraceId = INIT_ID;
    }

    bool isPlaying() const {
        return stream_ && stream_->getState() == oboe::StreamState::Started;
    }

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) override {
        const auto ts0 = schrc::now();
        auto refData = silentAudioRenderData;
        auto output = static_cast<int16_t *>(audioData);
        const auto ts1 = schrc::now();
        const auto underrunCount = stream_->getXRunCount().value();
//        std::this_thread::sleep_for(6ms); // STRESS
        const auto ts2 = schrc::now();
        while (underrunCount - underrunCountHandled > 0) {
            referenceQueue.push(underrunAudioRenderData);
            underrunCountHandled++;
        }
        const auto ts3 = schrc::now();
        [[maybe_unused]] const auto res = renderQueue.dropWhilePop(
                refData, [&](const AudioRenderDataHw &data) {
                    if (vad && data.meta.traceId != FILE_ID) {
                        traceIdToDrop = data.meta.traceId;
                        refData.interruptInitiator = INTERRUPT_VAD;
                        refData.traceIdToDrop = traceIdToDrop;
                    }
                    if (muted && data.meta.traceId != FILE_ID) {
                        traceIdToDrop = data.meta.traceId;
                        refData.interruptInitiator = INTERRUPT_MUTE;
                        refData.traceIdToDrop = traceIdToDrop;
                    }
                    auto gap = latestPlayedTraceId == SILENT_ID &&
                               data.meta.traceId <= latestAvatarTraceId;
                    if (gap && data.meta.traceId != FILE_ID) {
                        traceIdToDrop = data.meta.traceId;
                        refData.interruptInitiator = INTERRUPT_GAP;
                        refData.traceIdToDrop = traceIdToDrop;
                    }

                    return data.meta.traceId <= traceIdToDrop;
                });
        const auto ts4 = schrc::now();
        if (refData.meta.traceId != FILE_ID && refData.meta.traceId >= 0) {
            applyGain(refData.frame, calcTargetGain());
            refData.spkGain = currentGain;
            latestAvatarTraceId = refData.meta.traceId;
        } else resetGain();
        const auto ts5 = schrc::now();
        const int ch = audioStream ? audioStream->getChannelCount() : 1;
        if (ch <= 1) {
            // mono
            std::memcpy(output, refData.frame.data(), sizeof(int16_t) * refData.frame.size());
        } else {
            // stereo: duplicate mono frame into L/R
            for (size_t i = 0; i < refData.frame.size(); i++) {
                const auto s = refData.frame[i];
                output[i * 2] = s;
                output[i * 2 + 1] = s;
            }
        }
        latestPlayedTraceId = refData.meta.traceId;
        const auto ts6 = schrc::now();
        referenceQueue.push(refData);
        const auto ts7 = schrc::now();
        sc::duration<double, std::milli> elapsed[8]{ts1 - ts0, ts2 - ts1, ts3 - ts2, ts4 - ts3,
                                                    ts5 - ts4, ts6 - ts5, ts7 - ts6, ts7 - ts0};

        for (int i = 0; i < 8; ++i)
            if (elapsed[i] > renderElapsedMax[i])
                renderElapsedMax[i] = elapsed[i];

        if (logCounter++ % 2000 == 0) {
            myLog("underrunCount: %d\nunderrunCountHandled: %d\nduration/max:\n1-0: %f/%f\n2-1: %f/%f\n3-2: %f/%f\n4-3: %f/%f\n5-4: %f/%f\n6-5: %f/%f\n7-6: %f/%f\n7-0: %f/%f",
                  underrunCount, underrunCountHandled,
                  elapsed[0].count(), renderElapsedMax[0].count(),
                  elapsed[1].count(), renderElapsedMax[1].count(),
                  elapsed[2].count(), renderElapsedMax[2].count(),
                  elapsed[3].count(), renderElapsedMax[3].count(),
                  elapsed[4].count(), renderElapsedMax[4].count(),
                  elapsed[5].count(), renderElapsedMax[5].count(),
                  elapsed[6].count(), renderElapsedMax[6].count(),
                  elapsed[7].count(), renderElapsedMax[7].count()
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
    float attenuationFactor;
    float smoothing;
    float downSmoothing;
    float threshold;
    int downStartSamples;
    float currentGain = DEFAULT_SPK_GAIN;
    int downSamples = 0;

    std::atomic<float> vadConf{0};
    std::atomic<bool> vad{false};
    std::atomic<bool> muted{false};

    float calcTargetGain() {
        if (vadConf > threshold) {
            return std::pow(10.0f, (threshold - vadConf * attenuationFactor) / 20.0f);
        } else {
            return DEFAULT_SPK_GAIN;
        }
    }

    float slerp(float a, float b, float t) {
        float result = a + (b - a) * t;
        if (std::abs(a - result) < 0.0005f) {
            return a;
        } else {
            return result;
        }
    }

    void resetGain() {
        currentGain = DEFAULT_SPK_GAIN;
        downSamples = 0;
    }

    template<class T>
    void applyGain(T &frame, float target) {
        for (int16_t &s: frame) {
            bool down = target < currentGain;
            if (down) {
                downSamples = downSamples + 1;
            } else {
                downSamples = 0;
            }
            bool downStart = downSamples >= downStartSamples;

            if (down && downStart) {
                currentGain = slerp(target, currentGain, downSmoothing);
            } else if (down) {
                // explicit raw gain assignment
                currentGain = (currentGain);
            } else {
                currentGain = slerp(target, currentGain, smoothing);
            }

            float result = static_cast<float>(s) * currentGain;
            if (result < -32768.0f) {
                s = static_cast<int16_t>(-32768);
            } else if (result > 32767.0f) {
                s = static_cast<int16_t>(32767);
            } else {
                s = static_cast<int16_t>(result);
            }
        }
    }

    int32_t underrunCountHandled = 0;
    sc::duration<double, std::milli> renderElapsedMax[8]{};

    std::shared_ptr<oboe::AudioStream> stream_;

    int32_t sampleRate_;
    int32_t framesPerBuffer_;

    size_t logCounter = 0;

    AudioRenderDataMirrorBufferHw renderMirrorBuffer;
    AtomicQueue<AudioRenderDataHw, 8192> renderQueue;

    RefDataMirrorBufferHw referenceMirrorBuffer;
    AtomicQueue<RefDataHw, 256> referenceQueue;

    std::atomic<int32_t> traceIdToDrop{INIT_ID};
    std::atomic<int32_t> latestAvatarTraceId{INIT_ID};
    std::atomic<int32_t> latestPlayedTraceId{INIT_ID};
};
