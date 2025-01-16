#pragma once

#include <functional>
#include <vector>
#include <sstream>

#include "FrameAdapter.h"
#include "SoundBuffer.h"
#include "SpeakerTestCallback.h"
#include "utils/similarity.h"
#include "DeviceDuplex.h"

template<class Options>
class SpeakerTest {
public:

    SpeakerTest(DeviceDuplex<Options> &duplex,
                SafeJavaVM &vm,
                SoundBuffer<typename Options::SampleT> &sound,
                Storage &storage) :
            framed_sound(sound.data, sound.size),
            spk_queue(duplex.spk_queue),
            mic_queue(duplex.mic_queue),
            sync(duplex.sync),
            storage(storage) {}

    void start() {
        MY_DBG();
        if (sync.running) return;
        MY_DBG();
        if (handler_thread.joinable()) handler_thread.join();
        MY_DBG();
        sync.running = true;
        MY_DBG();
        handler_thread = std::thread(&SpeakerTest::threadRoutine, this);
        MY_DBG();
    }

    void stop() {
        if (!sync.running) return;
        sync.running = false;
        sync.cv.notify_one();
//      todo  if (handler_thread.joinable()) handler_thread.detach();
    }

    auto setOnFinish(std::function<void()> f) { onFinish = f; }

    auto setOnResultCalculated(std::function<void(uint32_t, double, double)> f) {
        onResultCalculated = f;
    }

private:
    void resetQueues() {
        spk_queue.reset();
        mic_queue.reset();
        for (size_t i = 0; i < Options::FrameCount / 2; ++i) {
            spk_queue.push(emptyFrame);
            mic_queue.push(emptyFrame);
        }
    }

    void threadRoutine() {
        MY_DBG();
        size_t data_counter{0};
        size_t capture_counter{0};
        auto frames_to_capture_count = framed_sound.frame_count + window;

        using namespace std::chrono;
        auto time = static_cast<uint32_t>(duration_cast<seconds>(
                system_clock::now().time_since_epoch()).count());

        std::vector<typename Options::FrameT> captured_data(frames_to_capture_count);

        sync();//wait for devices start
        resetQueues();

        while (sync.running && capture_counter < frames_to_capture_count) {
            myLog("%zu", data_counter);

            auto current_time = std::chrono::high_resolution_clock::now();
            if (data_counter < framed_sound.frame_count) {
                typename Options::FrameT &data_frame = framed_sound.data[data_counter];
                if (!spk_queue.push(data_frame))
                    myLog<Prio::E>("Test render overflow");
            } else {
                if (!spk_queue.push(emptyFrame))
                    myLog<Prio::E>("Test render overflow");
            }
            if (!mic_queue.pop(captured_data[capture_counter])) {
                myLog<Prio::E>("Test capture underflow");
            }

            data_counter++;
            capture_counter++;
            sync();
        }
        sync.running = false;
        onFinish();

        NullifyingRangeAdapter a(
                reinterpret_cast<typename Options::SampleT *>(framed_sound.data),
                framed_sound.frame_count * Options::FrameSize);

        NullifyingRangeAdapter b(
                reinterpret_cast<typename Options::SampleT *>(captured_data.data()),
                captured_data.size() * Options::FrameSize);

        auto result = similarity(a, b, window * Options::FrameSize);

        myLog("%zu, %f, %f", result.index, result.formFactor, result.scaleFactor);
        onResultCalculated(time, result.formFactor, result.scaleFactor);

//        std::stringstream log_f{};
//        log_f << "index,played,recorded\n";
//        for (size_t i = 0; i < b.size(); ++i) {
//            log_f << i << ',' << std::abs(a[i]) << ',' << std::abs(b[i + result.index]) << '\n';
//        }

//        storage.openDebugStream("played.raw").write(framed_sound.data,
//                                                    framed_sound.frame_count *
//                                                    sizeof(typename Options::FrameT));
//        storage.openDebugStream("captured.raw").write(captured_data);
//        storage.openDebugStream("speaker_test.csv").write(log_f.rdbuf()->str());
    }

    std::thread handler_thread;

    std::function<void()> onFinish{};
    std::function<void(uint32_t, double, double)> onResultCalculated{};
    FrameAdapter<Options> framed_sound;
    typename DeviceDuplex<Options>::queue_t &spk_queue;
    typename DeviceDuplex<Options>::queue_t &mic_queue;

    DuplexSync &sync;

    Storage &storage;

    constexpr static typename Options::FrameT emptyFrame{};
    constexpr static size_t window = Options::FrameCount * 2;
};