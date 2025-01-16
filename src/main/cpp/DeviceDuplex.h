#pragma once

#include "utils/common.h"
#include "Device.h"
#include "DuplexSync.h"
#include "DeviceInfo.h"

/*
 * Owns both input and output devices, queues and provides validated state changes for them.
 */
template<class Options>
struct DeviceDuplex {
    using spk_t = Device<Options, Dir::OUT>;
    using mic_t = Device<Options, Dir::IN>;
    using queue_t = typename Options::QueueT;

    enum class Cmd {
        openMicro,
        startMicro,
        stopMicro,
        closeMicro,
        openSpeaker,
        startSpeaker,
        stopSpeaker,
        closeSpeaker,
    };

    DeviceDuplex() : spk(spk_queue, sync), mic(mic_queue, sync) {}

    void operator()(const Cmd &cmd) {
        switch (cmd) {
            case Cmd::openMicro:
                if (micro_ready) return;
                std::tie(micro_ready, mic_info) = mic.open();
                myLog("Cmd::openMicro-> id = %i", mic_info.id);
                break;
            case Cmd::closeMicro:
                if (!micro_ready) return;
                mic.close();
                micro_ready = false;
                break;
            case Cmd::openSpeaker:
                if (speaker_ready) return;
                std::tie(speaker_ready, spk_info) = spk.open();
                myLog("Cmd::openSpeaker-> id = %i", spk_info.id);
                break;
            case Cmd::closeSpeaker:
                if (!speaker_ready) return;
                spk.close();
                speaker_ready = false;
                break;
            case Cmd::startMicro:
                if (micro_started) return;
                sync.spkPop = false;
                mic.start();
                micro_started = true;
                break;
            case Cmd::stopMicro:
                if (!micro_started) return;
                mic.stop();
                micro_started = false;
                break;
            case Cmd::startSpeaker:
                if (speaker_started) return;
                sync.micPush = false;
                spk.start();
                speaker_started = true;
                break;
            case Cmd::stopSpeaker:
                if (!speaker_started) return;
                spk.stop();
                speaker_started = false;
                break;
        }
        myLog("cmd: %s", cmd_name[static_cast<int>(cmd)]);
    }

    DeviceInfo spk_info{};
    DeviceInfo mic_info{};

    queue_t spk_queue;
    queue_t mic_queue;

    DuplexSync sync;
private:

    spk_t spk;
    mic_t mic;

    bool
            speaker_ready{false},
            speaker_started{false},
            micro_ready{false},
            micro_started{false};

    constexpr static std::array cmd_name{
            "Open Micro",
            "Start Micro",
            "stoP Micro",
            "Close Micro",
            "Open Speaker",
            "Start Speaker",
            "stoP Speaker",
            "Close Speaker",
    };
};