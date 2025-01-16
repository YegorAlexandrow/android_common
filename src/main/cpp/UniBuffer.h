#pragma once

#include "utils/common.h"
#include "utils/JavaObject.h"
#include <memory>
#include <cstddef>

template<size_t bytesPerFrame>
struct UniBuffer {
    using arr_t = std::array<uint8_t, bytesPerFrame>;

    explicit UniBuffer(SafeJavaVM &vm) :
            array(std::make_unique<arr_t>()),
            o(vm, vm.getEnv()->NewDirectByteBuffer(array->data(), array->size())) {
        MY_DBG();
    }

    void rewind() {
        MY_DBG();
        o.template call<MI::rewind>();
    }

    enum class MI {
        rewind,
    };

    std::unique_ptr<arr_t> array;
    JavaObject<MI, MethodDescription{MI::rewind, ReturnType<jobject>{}, "rewind",
                                     "()Ljava/nio/ByteBuffer;"}> o;

};
