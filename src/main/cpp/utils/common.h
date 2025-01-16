#pragma once

#include <android/log.h>
#include <jni.h>
#include <cstdint>
#include <tuple>
#include <thread>
#include <array>
#include <atomic>
#include <chrono>

enum class Prio {
    V = ANDROID_LOG_VERBOSE,
    D,
    I,
    W,
    E,
    F,
};

constexpr const char *TAG = "NativeAudio";

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"

template<Prio prio = Prio::V, size_t N, class...T>
inline auto myLog(const char (&fmt)[N], T &&...x) {
    return __android_log_print(static_cast<int>(prio), TAG, fmt, std::forward<T>(x)...);
}

inline thread_local unsigned long dbg_ctr{};

#define MY_DBG() ((void)(myLog("%lu %s, %i, %s",++dbg_ctr, __FILE__, __LINE__, __PRETTY_FUNCTION__)))
//#define MY_DBG()

#pragma clang diagnostic pop

static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }