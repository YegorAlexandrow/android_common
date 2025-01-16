#pragma once

#include <numeric>
#include <cmath>
#include <utility>
#include "common.h"
#include "NullifyingRangeAdapter.h"

template<class R = double>
struct SimilarityResult {
    size_t index;
    R formFactor, scaleFactor;
};
template<class R> SimilarityResult(size_t, R, R)->SimilarityResult<R>;

template<class T, class R = double>
auto vabs(const NullifyingRangeAdapter<T> &adapter, R r = {}) {
    return std::sqrt(
            std::accumulate(adapter.begin(), adapter.end(), r, [](const auto a, const auto b) {
                return std::move(a) + b * b;
            }));
}

template<class A, class B, class R = double>
auto vdotshift(const NullifyingRangeAdapter<A> &a, const NullifyingRangeAdapter<B> &b, int s = 0) {
    auto max = std::max(a.size(), b.size());
    R acum{};
    for (size_t i{}; i < max; i++) acum += a[i] * b[i + s];
    return acum;
}

template<class A, class B, class R = double>
auto
similarity(const NullifyingRangeAdapter<A> &a, const NullifyingRangeAdapter<B> &b, size_t window) {
    auto abs_a = vabs(a);
    auto abs_b = vabs(b);
    myLog("||| a: %f | b: %f |||", abs_a, abs_b);

    R max_value{};
    size_t max_value_index{};
    R current_value{};

    std::stringstream log_f{};
    log_f << "index,sim\n";

    for (size_t i{}; i < window; i++) {
        current_value = vdotshift(a, b, i);
        log_f << i << ',' << current_value << '\n';
        if (current_value > max_value) {
//            myLog("new max: %f", current_value);
            max_value = current_value;
            max_value_index = i;
        }
    }
//    std::ofstream{"/storage/emulated/0/Download/SIM_RECORD.csv"} << log_f.rdbuf()->str();

    return SimilarityResult{max_value_index, max_value / abs_a / abs_b, abs_b / abs_a};
}