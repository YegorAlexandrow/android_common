#pragma once

#include <array>
#include <mutex> //call_once
#include <memory> //unique_ptr

#include <oboe/Oboe.h>

#include "utils/AtomicQueue.h"
#include "utils/common.h"

enum class Dir {
    IN, OUT
};

struct DeviceOptions {
    using SampleT = int16_t;
    constexpr static oboe::AudioFormat format = oboe::AudioFormat::I16;
    constexpr static size_t
            Freq = 48'000,
            channelsCount = 1,
            FrameMultiplayer = 4,
            FrameSize = int((FrameMultiplayer * 16.0 * Freq * channelsCount) / 8000),
            FrameCount = 8,
            BytesPerSample = sizeof(SampleT),
            BytesPerFrame = BytesPerSample * FrameSize;
    using FrameT = std::array<SampleT, FrameSize>;
    using QueueT = AtomicQueue<FrameT, FrameCount>;
};
