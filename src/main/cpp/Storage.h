#pragma once

#include "utils/JavaObject.h"
#include "OutputStream.h"

struct Storage {
    enum class MI {
        openDebugStream
    };

    Storage(SafeJavaVM &vm, jobject jstorage) : vm(vm), o(vm, jstorage) {}

    OutputStream openDebugStream(const std::string &fileName) {
        return OutputStream(vm, o.call<MI::openDebugStream>(
                vm.getEnv()->NewStringUTF(fileName.c_str())));
    }

private:

    SafeJavaVM &vm;

    JavaObject<MI,
            MethodDescription{
                    MI::openDebugStream,
                    ReturnType<jobject>{},
                    "openDebugStream",
                    "(Ljava/lang/String;)Ljava/io/OutputStream;"
            }
    > o;
};
