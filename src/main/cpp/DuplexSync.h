#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

struct DuplexSync {
    std::atomic<bool>
            micPush{0},
            spkPop{0},
            running{0};
    std::mutex m;
    std::condition_variable cv;

    bool predicate() {
        return !running || (spkPop && micPush);
    }

    void operator()() {
        if (!predicate()) {
            std::unique_lock lk(m);
            cv.wait(lk, [&] {
                return predicate();
            });
        }
        spkPop = false;
        micPush = false;
    }
};
