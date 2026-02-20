#pragma once

#include <cstddef>
#include <memory>
#include "common.h"
#include "SafeJavaVM.h"
#include "JavaObject.h"

constexpr inline int32_t FILE_ID = INT32_MAX;
constexpr inline int32_t SILENT_ID = -1;
constexpr inline int32_t UNDERRUN_ID = -2;
constexpr inline int32_t INIT_ID = -3;

constexpr inline size_t RENDER_FRAME_SAMPLES_AEC = 240;
constexpr inline size_t CAPTURE_FRAME_SAMPLES_AEC = 320;
constexpr inline size_t OUT_FRAME_SAMPLES_AEC = 160;
constexpr inline size_t RENDER_FRAME_SAMPLES_HW = RENDER_FRAME_SAMPLES_AEC * 8 / 10;
constexpr inline size_t CAPTURE_FRAME_SAMPLES_HW = CAPTURE_FRAME_SAMPLES_AEC * 8 / 10;
constexpr inline size_t OUT_FRAME_SAMPLES_HW = OUT_FRAME_SAMPLES_AEC * 8 / 10;

struct AudioRenderMeta {
    int32_t traceId = INIT_ID;
    int32_t spkId = INIT_ID;
    int32_t sliceId = INIT_ID;
};

template<size_t frameSizeSamples>
struct AudioRenderData {
    std::array<int16_t, frameSizeSamples> frame{};
    AudioRenderMeta meta{};
};

template<size_t frameSizeSamples>
struct AudioCaptureData {
    std::array<int16_t, frameSizeSamples> frame{};
};

template<class T>
struct MirrorBuffer {

    explicit MirrorBuffer(SafeJavaVM &vm) :
            data(std::make_unique<T>()),
            o(vm, vm.getEnv()->NewDirectByteBuffer(data.get(), sizeof(T))) {
        MY_DBG();
    }

    void rewind() {
        MY_DBG();
        o.template call<MI::rewind>();
    }

    enum class MI {
        rewind,
    };

    std::unique_ptr<T> data;
    JavaObject<MI, MethodDescription{MI::rewind, ReturnType<jobject>{}, "rewind",
                                     "()Ljava/nio/ByteBuffer;"}> o;

};


using AudioRenderDataHw = AudioRenderData<RENDER_FRAME_SAMPLES_HW>;
using AudioCaptureDataHw = AudioCaptureData<CAPTURE_FRAME_SAMPLES_HW>;

using AudioRenderDataMirrorBufferHw = MirrorBuffer<AudioRenderDataHw>;
using AudioCaptureDataMirrorBufferHw = MirrorBuffer<AudioCaptureDataHw>;

constexpr inline AudioRenderDataHw silentAudioRenderData{
        {}, AudioRenderMeta{SILENT_ID, SILENT_ID, SILENT_ID}
};
constexpr inline AudioRenderDataHw underrunAudioRenderData{
        {}, AudioRenderMeta{UNDERRUN_ID, UNDERRUN_ID, UNDERRUN_ID}
};
