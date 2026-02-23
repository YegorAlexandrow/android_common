#pragma once

#include <atomic>
#include <cstdint>
#include "common.h"

template<class T, uint32_t N>
class AtomicQueue {
public:

    static_assert(
            isPowerOfTwo(N),
            "Capacity must be a power of 2"
    );

    template<class Dropper>
    bool dropWhilePop(T &val, const Dropper &dropper) {
        while (!isEmpty() && dropper(std::as_const(buffer[mask(readCounter)]))) {
            ++readCounter;
        }
        if (isEmpty() or dropper(std::as_const(buffer[mask(readCounter)]))) {
            return false;
        } else {
            val = buffer[mask(readCounter)];
            ++readCounter;
            return true;
        }
    }

    void reset() {
        writeCounter = 0;
        readCounter = 0;
    }

    bool pop(T &val) {
        if (isEmpty()) {
            return false;
        } else {
            val = buffer[mask(readCounter)];
            ++readCounter;
            return true;
        }
    }

    bool push(const T &item) {
        if (isFull()) {
            return false;
        } else {
            buffer[mask(writeCounter)] = item;
            ++writeCounter;
            return true;
        }
    }

    uint32_t size() const {
        return writeCounter - readCounter;
    };

    bool blockingPop(T &val) {
        writeCounter.wait(readCounter);
        return pop(val);
    }

    bool pushNotify(T &item) {
        auto res = push(item);
        writeCounter.notify_one();
        return res;
    }

private:

    bool isEmpty() const { return readCounter == writeCounter; }

    bool isFull() const { return size() == N; }

    uint32_t mask(uint32_t n) const { return static_cast<uint32_t>(n & (N - 1)); }

    T buffer[N];
    std::atomic<uint32_t> writeCounter{0};
    std::atomic<uint32_t> readCounter{0};

};
