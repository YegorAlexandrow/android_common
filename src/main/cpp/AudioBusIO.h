#pragma once

#include <android/log.h>

#include "UniBuffer.h"
#include "utils/JavaObject.h"

template<class Options>
struct AudioBusIO {
    UniBuffer<Options::BytesPerFrame> read_buf;
    UniBuffer<Options::BytesPerFrame> write_buf;

    enum class MI {
        writeCaptureData,
        readRenderData,
    };

    JavaObject<MI,
            MethodDescription{
                    MI::writeCaptureData, ReturnType<void>{}, "writeCaptureData",
                    "(Ljava/nio/ByteBuffer;I)V"},
            MethodDescription{
                    MI::readRenderData, ReturnType<jint>{}, "readRenderData",
                    "(Ljava/nio/ByteBuffer;I)I"}
    > o;


    AudioBusIO(SafeJavaVM &vm, jobject jbus) :
            read_buf(vm),
            write_buf(vm),
            o(vm, jbus) {
        myLog(__PRETTY_FUNCTION__);
    }

    void write(uint8_t *from) {
        if (!capture_enabled) return;
        std::copy(from, from + write_buf.array->size(), write_buf.array->begin());
        flush();
    }

    [[nodiscard]] jint read() {
        if (!render_enabled) return 0;

        return o.template call<MI::readRenderData>(
                *read_buf.o,
                read_buf.array->size() / Options::BytesPerSample);
    }

    void setRenderState(bool state) {
        render_enabled = state;
    }

    void setCaptureState(bool state) {
        capture_enabled = state;
    }

private:

    std::atomic<bool>
            render_enabled = false,
            capture_enabled = false;

    void flush() {
        write_buf.rewind();
        o.template call<MI::writeCaptureData>(
                *write_buf.o,
                write_buf.array->size() / Options::BytesPerSample);
    }

};

