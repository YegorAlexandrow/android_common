#pragma once

#include <tuple>

template<class T>
struct ReturnType {
};

template<class MI, class R, size_t N, size_t S>
struct MethodDescription {
    using r_T = R;
    MI mi;
    ReturnType<R> returnType;
    char name[N];
    char sig[S];
};

template<class MI, class R, size_t N, size_t S>
MethodDescription(const MI &, const ReturnType<R> &, const char(&)[N],
                  const char(&)[S])->MethodDescription<MI, R, N, S>;

template<class MI, MethodDescription...mdv>
class JavaObject {
    template<MI mi> using R =
            typename std::tuple_element_t<
                    static_cast<size_t>(mi),
                    std::tuple<decltype(mdv)...>
            >::r_T;
public:
    JavaObject(SafeJavaVM &vm, const jobject &ref) :
            vm(vm), globalRef(vm.getEnv()->NewGlobalRef(ref)),
            classRef(reinterpret_cast<jclass>(vm.getEnv()->NewGlobalRef(
                    vm.getEnv()->GetObjectClass(globalRef)))) {
        MY_DBG();
        registerMethods(std::tuple{mdv...});
    }

    ~JavaObject() {
        MY_DBG();
        vm.getEnv()->DeleteGlobalRef(globalRef);
        vm.getEnv()->DeleteGlobalRef(classRef);
    }

    jobject operator*() { return globalRef; }

    template<MI mi, class...Args>
    R<mi> call(Args &&... args) {
        MY_DBG();
        static_assert(
                std::get<static_cast<size_t>(mi)>(std::tuple{mdv...}).mi == mi,
                "invalid method");
        return call_impl<R<mi>>(globalRef, method[static_cast<size_t>(mi)],
                                std::forward<Args>(args)...);
    }

private:
    template<class T, class...Args>
    T call_impl(Args &&...args) const {
        MY_DBG();
        if constexpr (std::is_same_v<T, void>)
            return vm.getEnv()->CallVoidMethod(std::forward<Args>(args)...);
        else if constexpr (std::is_same_v<T, jint>)
            return vm.getEnv()->CallIntMethod(std::forward<Args>(args)...);
        else if constexpr (std::is_same_v<T, jobject>)
            return vm.getEnv()->CallObjectMethod(std::forward<Args>(args)...);
        else static_assert("incorrect return type");
    }

    template<class MDT, std::size_t... Is>
    void registerMethods_impl(const MDT &tp, std::index_sequence<Is...>) {
        MY_DBG();
        ((method[Is] = vm.getEnv()->GetMethodID(classRef, std::get<Is>(tp).name,
                                                std::get<Is>(tp).sig)), ...);
    }

    template<class MDT, std::size_t size = std::tuple_size_v<MDT>>
    void registerMethods(const MDT &tp) {
        MY_DBG();
        registerMethods_impl(tp, std::make_index_sequence<size>{});
    }


private:
    SafeJavaVM &vm;

    jobject globalRef{};
    jclass classRef{};

    jmethodID method[sizeof...(mdv)]{};
};