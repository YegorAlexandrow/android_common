#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "DeviceDuplex.h"
#include "AudioBusIO.h"
#include "aec.h"

#include "utils/AtomicQueue.h"
#include "utils/SafeJavaVM.h"
#include "Storage.h"

template<class Options>
class CallAec {
public:
    CallAec(DeviceDuplex<Options> &duplex, SafeJavaVM &vm, jobject jbus, Storage &storage)
            : spk_queue(duplex.spk_queue), mic_queue(duplex.mic_queue), sync(duplex.sync),
              bus(vm, jbus), storage(storage) {}

    void start() {
        if (sync.running) return;
        sync.running = true;
        handler_thread = std::thread(&CallAec::threadRoutine, this);
        myLog("callAed start");
    }

    void stop() {
        if (!sync.running) return;
        sync.running = false;
        sync.cv.notify_one();
        if (handler_thread.joinable()) handler_thread.detach();
        myLog("callAed stop");
    }

    void setOnFinish(std::function<void()> f) { onFinish = f; }

private:

    void resetQueues() {
        spk_queue.reset();
        mic_queue.reset();
        for (size_t i = 0; i < Options::FrameCount / 2; ++i) {
            spk_queue.push(emptyFrame);
            mic_queue.push(emptyFrame);
        }
    }

    void reset() {
        stabilization_muting_counter = stabilization_muting;
        resetQueues();
        aec::restart();
    }

    void handleSpeaker() {
        static typename Options::FrameT frame;
        [[maybe_unused]] auto read_bytes = bus.read();
        memcpy(frame.data(), bus.read_buf.array->data(),
               Options::BytesPerFrame);
        if (stabilization_muting_counter) {
            frame.fill(0);
        }
        if (aec_on) {
            auto aec_res = aec::processSpeaker(
                    Options::BytesPerFrame,
                    reinterpret_cast<BYTE *>(frame.data()));
            if (aec_res != SOLICALL_RC_SUCCESS) {
                if (aec_res == SOLICALL_RC_RESTARTED) {
                    frame.fill(0);
                    reset();
                    spk_queue.push(frame);
                } else {
                    myLog<Prio::E>("aec::processSpeaker error %i", aec_res);
                    aec_on = false;
                }
            }
        }
        if (!spk_queue.push(frame)) {
            myLog<Prio::E>("Vonage render frame lost");
        }
        frame.fill(0);
    }


    void handleMicro() {
        static typename Options::FrameT frame;
        if (!mic_queue.pop(frame)) {
            myLog<Prio::E>("Vonage capture underflow");
        }
        if (aec_on) {
            [[maybe_unused]] int filtered_bytes_count;

            auto aec_res = aec::processMicro(
                    Options::BytesPerFrame,
                    reinterpret_cast<BYTE *>(frame.data()),
                    reinterpret_cast<BYTE *>(frame.data()),
                    filtered_bytes_count);

            if (aec_res != SOLICALL_RC_SUCCESS) {
                if (aec_res == SOLICALL_RC_RESTARTED) {
                    frame.fill(0);
                    bus.write(reinterpret_cast<uint8_t *>(frame.data()));
                    reset();
                } else {
                    myLog<Prio::E>("aec::processSpeaker error %i", aec_res);
                    aec_on = false;
                }
            }
        }
        if (stabilization_muting_counter) {
            frame.fill(0);
        }
        bus.write(reinterpret_cast<uint8_t *>(frame.data()));
        frame.fill(0);
    }

    void threadRoutine() {
        myLog("CallAec threadRoutine started");
        aec_on = (aec::init() == SOLICALL_RC_SUCCESS);
        stabilization_muting_counter = 0;
        std::stringstream log_f{};
        log_f << "index, spk_fill, mic_fill, delta_ms\n";

        size_t log_counter{};
        sync();  //wait for devices start
        resetQueues();
        using namespace std::chrono;
        auto past_time = high_resolution_clock::now();

        while (sync.running) {

            if (log_counter < 100'000) {
                auto now_time = high_resolution_clock::now();
                log_f << log_counter++ << ',' << spk_queue.size() << ',' << mic_queue.size() << ','
                      << duration_cast<nanoseconds>(now_time - past_time).count() / 1000'000.0
                      << '\n';
                past_time = now_time;
            }

            handleSpeaker();
            handleMicro();

            if (spk_queue.size() < Options::FrameCount / 2) {
                handleSpeaker();
                myLog("double speaker");
            }
            if (mic_queue.size() > Options::FrameCount / 2) {
                handleMicro();
                myLog("double micro");
            }

            if (stabilization_muting_counter) {
                --stabilization_muting_counter;
            }

            sync();
        }

        aec::destroy();
        onFinish();

//        storage.openDebugStream("call_queues.csv").write(log_f.rdbuf()->str());

    }

    std::function<void()> onFinish{};
    std::thread handler_thread;

    typename DeviceDuplex<Options>::queue_t &spk_queue;
    typename DeviceDuplex<Options>::queue_t &mic_queue;

    DuplexSync &sync;
    bool aec_on{false};

    size_t
            stabilization_muting{0xff},
            stabilization_muting_counter{0};

    static typename Options::FrameT constexpr emptyFrame{};

public: // todo
    AudioBusIO<Options> bus;
private:
    Storage &storage;

};