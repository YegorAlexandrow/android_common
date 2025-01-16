#pragma once

#include "utils/common.h"

template<class Options>
struct FrameAdapter {
    FrameAdapter() : data(), frame_count() {}

    FrameAdapter(typename Options::SampleT *data, size_t size) : data(
            reinterpret_cast<typename Options::FrameT *>(data)), frame_count(
            size / Options::FrameSize) {}

    typename Options::FrameT *data;
    size_t frame_count;
};