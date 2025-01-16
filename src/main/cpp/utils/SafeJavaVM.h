#pragma once

#include "common.h"

//todo move to .cpp
namespace {
    JavaVM *raw_vm{};
    std::mutex vm_mutex;

    struct EnvRaii {
        JNIEnv *env{};
        bool external{};

        explicit EnvRaii(bool external = 0) : external(external) {
            MY_DBG();
            std::lock_guard lg(vm_mutex);
            raw_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
            if (!external) {
                auto res = raw_vm->AttachCurrentThread(&env, nullptr);
                if (res != 0) throw std::runtime_error("RaiiJvmThreadAttachment: Failed to attach");
                myLog("RaiiJvmThreadAttachment: new env created at %p", (void *) env);
            }
        };

        ~EnvRaii() {
            MY_DBG();
            std::lock_guard lg(vm_mutex);
            if (!external) {
                auto res = raw_vm->DetachCurrentThread();
                myLog("RaiiJvmThreadAttachment: thread finished with code %i", res);
            }
        }
    };

    thread_local std::unique_ptr<EnvRaii> tl_env{};
}

class SafeJavaVM {
public:
    explicit SafeJavaVM(JavaVM *vm_) {
        MY_DBG();
        raw_vm = vm_;
        myLog(__PRETTY_FUNCTION__);
    }

    template<class F>
    auto operator<<(F &&f) {
        MY_DBG();
        std::lock_guard lg(vm_mutex);
        return f(raw_vm);
    }

    ~SafeJavaVM() {
        MY_DBG();
        *this << [](...) {};
    }

    JNIEnv *getEnv() {
        MY_DBG();
        if (!tl_env) {
            tl_env = std::make_unique<EnvRaii>();
        }
        return tl_env->env;
    }

    void passExtEnv() {
        MY_DBG();
        if (!tl_env) {
            tl_env = std::make_unique<EnvRaii>(true);
        }
    }
};
