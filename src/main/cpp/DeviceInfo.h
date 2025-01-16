#pragma once

struct DeviceInfo {
    const char *api;
    int32_t
            id,
            bytesPerSample,
            bytesPerFrame,
            framesPerBurst,
            framesPerDataCallback,
            sampleRate;
};