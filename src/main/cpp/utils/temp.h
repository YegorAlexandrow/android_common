#pragma once

#include <cstddef>
#include <memory>
#include "common.h"
#include "SafeJavaVM.h"
#include "JavaObject.h"

namespace sc = std::chrono;
using schrc = sc::high_resolution_clock;

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

constexpr inline int32_t INTERRUPT_INIT = 0;
constexpr inline int32_t INTERRUPT_MUTE = 1;
constexpr inline int32_t INTERRUPT_VAD = 2;

constexpr inline float DEFAULT_SPK_GAIN = 1.0f;

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
struct RefData : AudioRenderData<frameSizeSamples> {
    int32_t interruptInitiator = INTERRUPT_INIT;
    int32_t traceIdToDrop = INIT_ID;
    float spkGain = DEFAULT_SPK_GAIN;
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
using RefDataHw = RefData<RENDER_FRAME_SAMPLES_HW>;
using AudioCaptureDataHw = AudioCaptureData<CAPTURE_FRAME_SAMPLES_HW>;

using AudioRenderDataMirrorBufferHw = MirrorBuffer<AudioRenderDataHw>;
using RefDataMirrorBufferHw = MirrorBuffer<RefDataHw>;
using AudioCaptureDataMirrorBufferHw = MirrorBuffer<AudioCaptureDataHw>;

constexpr inline RefDataHw silentAudioRenderData{
        {{}, {SILENT_ID, SILENT_ID, SILENT_ID}},
};
constexpr inline RefDataHw underrunAudioRenderData{
        {{}, {UNDERRUN_ID, UNDERRUN_ID, UNDERRUN_ID}},
};


std::string vectorToString(const std::vector<int16_t> &vec) {
    std::stringstream ss;
    for (size_t i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i < vec.size() - 1) {
            ss << " ";  // Separate each number with a space
        }
    }
    return ss.str();
}

// Load PCM16 data from a file
std::vector<int16_t> LoadPCM16File(const std::string &filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<int16_t> buffer(size / sizeof(int16_t));
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + filepath);
    }

    return buffer;
}

// Save PCM16 data to a file
void SavePCM16File(const std::string &filepath, const std::vector<int16_t> &data) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    if (!file.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(int16_t))) {
        throw std::runtime_error("Failed to write file: " + filepath);
    }
}
