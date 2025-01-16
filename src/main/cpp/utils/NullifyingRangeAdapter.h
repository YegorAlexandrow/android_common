#pragma once

#include <iostream>
#include <vector>
#include <numeric>
#include <iterator>

template<class T>
struct NullifyingRangeAdapter {
    using value_type = typename T::value_type;

    explicit NullifyingRangeAdapter(T &object) : object(object) {}

    const auto &begin() const { return std::begin(object); }

    const auto &end() const { return std::end(object); }

    auto size() const { return std::size(object); }

    const auto &operator[](size_t i) const {
        return i < std::size(object) ? object[i] : null;
    }

private:
    T &object;
    value_type null{};
};

template<class U>
struct NullifyingRangeAdapter<U *> {
    using value_type = U;

    NullifyingRangeAdapter(U *data, size_t size) : _begin(data), _size(size) {}

    const U *begin() const { return _begin; }

    const U *end() const { return _begin + _size; }

    auto size() const { return _size; }


    const U &operator[](size_t i) const { return i < _size ? _begin[i] : null; }

private:
    U *_begin;
    size_t _size;
    value_type null{};
};

template<class T> NullifyingRangeAdapter(T *, size_t) -> NullifyingRangeAdapter<T *>;
