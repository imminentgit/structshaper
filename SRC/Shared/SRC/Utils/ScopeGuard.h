#pragma once
#include <functional>

template <typename T, typename... TArgs>
class ScopeGuard {
    using args_tuple_type = std::tuple<TArgs...>;

    T _callback{};
    args_tuple_type _args{};
public:
    ScopeGuard(T&& callback, const TArgs&... args) : _callback(std::move(callback)), _args(args...) {}

    ~ScopeGuard() {
        std::apply(_callback, _args);
    }
};

template <typename T>
struct ScopedVariable {
    T& var{};
    T old_val{};

    ScopedVariable(T& var, T new_val) : var(var), old_val(var) {
        var = new_val;
    }

    ~ScopedVariable() {
        restore();
    }

    void restore() {
        var = old_val;
    }
};
