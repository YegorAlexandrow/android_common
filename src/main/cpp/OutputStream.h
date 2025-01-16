#pragma once

#include "utils/JavaObject.h"
#include <span>

struct OutputStream {
    enum class MI {
        write,
        close
    };

    OutputStream(SafeJavaVM &vm, jobject joutstream) : is_open(true), vm(vm), o(vm, joutstream) {}

    void write(const std::string &data) {
        write(static_cast<const void *>(data.c_str()), data.size());
    }

    template<class T>
    void write(const std::vector<T> &data) {
        write(data.data(), data.size() * sizeof(T));
    }

    template<class T>
    void write(std::span<T> data) {
        write(data.data(), data.size() * sizeof(T));
    }

    void write(const void *data, size_t count) {
        auto a = vm.getEnv()->NewByteArray(count);
        vm.getEnv()->SetByteArrayRegion(
                a, 0, count, reinterpret_cast<const jbyte *>(data));
        o.call<MI::write>(a);
    }


    void close() {
        is_open = false;
        o.call<MI::close>();
    }


    ~OutputStream() {
        if (is_open) close();
    }

private:
    std::atomic<bool> is_open;

    SafeJavaVM &vm;

    JavaObject<MI,
            MethodDescription{
                    MI::write,
                    ReturnType<void>{},
                    "write",
                    "([B)V"
            },
            MethodDescription{
                    MI::close,
                    ReturnType<void>{},
                    "close",
                    "()V"
            }
    > o;
};