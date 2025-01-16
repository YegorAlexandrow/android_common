#pragma once

#include <cstring>
#include "DeviceOptions.h"
#include "DeviceInfo.h"
#include "DuplexSync.h"

using namespace oboe;
using CbRes = DataCallbackResult;

template<class Options, Dir dir>
class Device {
public:
    explicit Device(typename Options::QueueT &queue_, DuplexSync &sync) :
            queue(queue_), sync(sync) {}

    std::pair<bool, DeviceInfo> open() {
        myLog("AudioDevice opening...");

        DeviceInfo info{};

        errorCallback = std::make_shared<ErrorCallback>(this);
        callBack = std::make_shared<Callback>(this);

        AudioStreamBuilder builder;
        builder.setAudioApi(AudioApi::AAudio)
                ->setChannelCount(Options::channelsCount)
                ->setSampleRate(Options::Freq)
                ->setFramesPerDataCallback(Options::FrameSize)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setErrorCallback(errorCallback)->setDataCallback(callBack)
                ->setFormat(Options::format);
        if constexpr(dir == Dir::IN)
            builder.setDirection(oboe::Direction::Input);
        else
            builder.setSharingMode(oboe::SharingMode::Exclusive);

        auto result = builder.openStream(stream);
        if (result != oboe::Result::OK) {
            myLog("Stream builder result: %s",
                  oboe::convertToText(result));
            return std::make_pair(false, info);
        }

        info.api = oboe::convertToText(stream->getAudioApi());
        info.id = stream->getDeviceId();
        info.bytesPerSample = stream->getBytesPerSample();
        info.bytesPerFrame = stream->getBytesPerFrame();
        info.framesPerBurst = stream->getFramesPerBurst();
        info.framesPerDataCallback = stream->getFramesPerDataCallback();
        info.sampleRate = stream->getSampleRate();

        myLog("Api: %s \tID: %i \tbytesPerSample: %i \tbytesPerFrame: %i \t"
              "framesPerBurst: %i \tgetFramesPerDataCallback: %i \tsampleRate: %i",
              info.api,
              info.id,
              info.bytesPerSample,
              info.bytesPerFrame,
              info.framesPerBurst,
              info.framesPerDataCallback,
              info.sampleRate
        );

        return std::make_pair(true, info);
    }

    bool start() {
        auto result = stream->requestStart();
        if (result != oboe::Result::OK) {
            myLog("Stream start result: %s",
                  oboe::convertToText(result));
            return false;
        }
        return true;
    }

    bool stop() {
        auto result = stream->requestStop();
        if (result != oboe::Result::OK) {
            myLog("Stream stop result: %s",
                  oboe::convertToText(result));
            return false;
        }
        return true;
    }

    bool close() {
        auto result = stream->close();
        if (result != oboe::Result::OK) {
            myLog("Stream stop result: %s",
                  oboe::convertToText(result));
            return false;
        }
        return true;
    }

private:

    struct Callback : public AudioStreamDataCallback {
        explicit Callback(Device *parent_) : parent(parent_) {}

        CbRes onAudioReady(AudioStream *, void *audioData, int32_t) override {
            auto data = static_cast<typename Options::FrameT *>(audioData);

            if constexpr (dir == Dir::IN) {
                if (!parent->queue.push(*data)) {
                    myLog<Prio::E>("MIC frame lost %zu", counter++);
                }
                parent->sync.micPush = true;

            } else {
                if (!parent->queue.pop(*data)) {
                    std::memset(data, typename Options::SampleT{}, Options::FrameSize);
                    myLog<Prio::E>("SPK frame underflow %zu", counter++);
                }
                parent->sync.spkPop = true;
            }

            parent->sync.cv.notify_one();

            return CbRes::Continue;
        }

    private:
        size_t counter{};
        Device *parent;
    };

    struct ErrorCallback : public AudioStreamErrorCallback {
        explicit ErrorCallback(Device *parent_) : parent(parent_) {}

        ~ErrorCallback() override = default;

        void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
            myLog<Prio::E>("oboe error: %s", oboe::convertToText(error));
            if (oboeStream->open() == Result::OK) oboeStream->requestStart();
        }

    private:
        Device *parent;
    };

    std::shared_ptr<oboe::AudioStream> stream;
    std::shared_ptr<ErrorCallback> errorCallback;
    std::shared_ptr<Callback> callBack;

    typename Options::QueueT &queue;

    DuplexSync &sync;
};