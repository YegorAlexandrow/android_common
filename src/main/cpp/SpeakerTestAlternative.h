#pragma once

#include <functional>
#include <vector>
#include <sstream>

#include "FrameAdapter.h"
#include "SoundBuffer.h"
#include "SpeakerTestCallback.h"
#include "utils/similarity.h"
#include "DeviceDuplex.h"
#include "Storage.h"

template<class Options>
class SpeakerTestAlternative {
public:

    SpeakerTestAlternative(DeviceDuplex<Options> &duplex, SafeJavaVM &vm, Storage &storage)
            : spk_queue(duplex.spk_queue),
              mic_queue(duplex.mic_queue),
              sync(duplex.sync), storage(storage) {}

    void start() {
        if (sync.running) return;
        sync.running = true;
        handler_thread = std::thread(&SpeakerTestAlternative::threadRoutine, this);
    }

    void stop() {
        if (!sync.running) return;
        sync.running = false;
        sync.cv.notify_one();
        if (handler_thread.joinable()) handler_thread.detach();
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
            spk_queue.push(emptyTestFrame);
            mic_queue.push(emptyTestFrame);
        }
    }

    void threadRoutine() {
        size_t capture_counter{0};

        using namespace std::chrono;
        auto time = static_cast<uint32_t>(duration_cast<seconds>(
                system_clock::now().time_since_epoch()).count());

//        auto test_frames = genDoubleHalf(time);
        auto test_frames = genImpulsePerCountSequence<0xff, 0xf>();


        auto frames_to_capture_count = audio_bit_width * test_frames.size() + window;
        std::vector<typename Options::FrameT> played_data(frames_to_capture_count);
        std::vector<typename Options::FrameT> captured_data(frames_to_capture_count);

        sync();//wait for devices start
        resetQueues();

        while (sync.running && capture_counter < frames_to_capture_count) {

            typename Options::FrameT const *frame{};
            auto next_frame = capture_counter / audio_bit_width;
            if (next_frame < test_frames.size())
                played_data[capture_counter] = *(frame = test_frames[next_frame]);
            else frame = &emptyTestFrame;

            myLog("%zu \t %p", next_frame, frame);

            if (!spk_queue.push(*frame))
                myLog<Prio::E>("Test render overflow");

            if (!mic_queue.pop(captured_data[capture_counter]))
                myLog<Prio::E>("Test capture underflow");


            capture_counter++;
            sync();
        }
        sync.running = false;
        onFinish();

        NullifyingRangeAdapter a(
                reinterpret_cast<typename Options::SampleT *>(played_data.data()),
                played_data.size() * Options::FrameSize);

        NullifyingRangeAdapter b(
                reinterpret_cast<typename Options::SampleT *>(captured_data.data()),
                captured_data.size() * Options::FrameSize);

        auto result = similarity(a, b, window * Options::FrameSize);

        myLog("%zu, %f, %f", result.index, result.formFactor, result.scaleFactor);
        onResultCalculated(time, result.formFactor, result.scaleFactor);

//        std::stringstream log_f{};
//        log_f << "index,played,recorded\n";
//        for (size_t i = 0; i < a.size(); ++i) {
//            log_f << i << ',' << std::abs(a[i]) << ',' << std::abs(b[i + result.index]) << '\n';
//        }
//
//        storage.openDebugStream("played.raw").write(played_data);
//        storage.openDebugStream("captured.raw").write(captured_data);
//        storage.openDebugStream("speaker_test.csv").write(log_f.rdbuf()->str());
    }

    std::thread handler_thread;

    std::function<void()> onFinish{};
    std::function<void(uint32_t, double, double)> onResultCalculated{};

    typename DeviceDuplex<Options>::queue_t &spk_queue;
    typename DeviceDuplex<Options>::queue_t &mic_queue;

    DuplexSync &sync;

    Storage &storage;

    static auto constexpr genSquareWave(size_t samples_per_cycle, size_t scale_decrease_div) {
        typename Options::FrameT rt{};
        size_t counter{};
        bool side{};
        for (auto &s: rt) {
            if (counter == samples_per_cycle) {
                counter = 0;
                side = !side;
            }
            s = side
                ? std::numeric_limits<typename Options::SampleT>::max()
                : std::numeric_limits<typename Options::SampleT>::lowest();
            s = s / scale_decrease_div;
            ++counter;
        }
        for (size_t i = 0; i < rt.size() / 2; ++i) {
            rt[i] = static_cast<double>(rt[i]) * i / rt.size();
            rt[rt.size() - 1 - i] = static_cast<double>(rt[rt.size() - 1 - i]) * i / rt.size();
        }
        return rt;
    }

    static auto constexpr genUniSquareImpulse(size_t samples_length) {
        typename Options::FrameT rt{};
        size_t counter{};
        for (auto &s: rt)
            if (counter++ < samples_length)
                s = std::numeric_limits<typename Options::SampleT>::max();
        return rt;
    }

    static typename Options::FrameT constexpr emptyTestFrame{};
    static typename Options::FrameT constexpr filledTestFrame{
            genSquareWave(Options::FrameSize / 128, 2)};

    static typename Options::FrameT constexpr UniSquareImpulseFrame{
            genUniSquareImpulse(Options::FrameSize / 16)};

    template<class T>
    static auto constexpr genSequence(T num) {
        constexpr size_t bites = sizeof(T) * 8;
        std::array<const typename Options::FrameT *, bites> seq{};
        for (auto &f: seq) f = &emptyTestFrame;
        for (int i = bites - 1; i >= 0; i--)
            if ((num >> i) & 1) {
                seq[bites - i - 1] = &filledTestFrame;
            }
        return seq;
    }

    template<size_t FramesCount>
    static auto constexpr genImpulseSequence() {
        std::array<const typename Options::FrameT *, FramesCount> seq{};
        for (auto &f: seq) f = &UniSquareImpulseFrame;
        return seq;
    }

    template<size_t FramesCount, size_t Count>
    static auto constexpr genImpulsePerCountSequence() {
        std::array<const typename Options::FrameT *, FramesCount> seq{};
        size_t counter{};
        for (auto &f: seq) {
            if (counter % Count == 0) f = &UniSquareImpulseFrame;
            else f = &emptyTestFrame;
        }

        return seq;
    }

    template<class T>
    static auto constexpr genDoubleHalf(T num) {
        auto seq = genSequence(num);
        for (size_t i = 0; i < seq.size() / 2; ++i) {
            seq[i] = seq[i + seq.size() / 2];
        }
        return seq;
    }

    constexpr static auto maxSample = std::numeric_limits<typename Options::SampleT>::max();
    constexpr static auto minSample = std::numeric_limits<typename Options::SampleT>::lowest();

    constexpr static size_t audio_bit_width = 0x1;
    constexpr static size_t threshold =
            maxSample >> 6;


    constexpr static size_t window = Options::FrameCount * 2;
    static_assert(
            isPowerOfTwo(audio_bit_width),
            "audio bit wide must be a power of 2"
    );
};