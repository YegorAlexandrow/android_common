#pragma once

#include "utils/common.h"

template<class SampleT>
struct SoundBuffer {
    SampleT *data;
    size_t size;

    SoundBuffer() {
        MY_DBG();
    }

    SoundBuffer(void *buf, size_t size) : data(static_cast<SampleT *>(buf)),
                                          size(size / sizeof(SampleT)) {
        MY_DBG();
    }
};