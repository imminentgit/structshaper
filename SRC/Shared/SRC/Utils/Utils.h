#pragma once

#include <string>

namespace Utils {
    std::string wstring_to_string(std::wstring_view in);

    std::wstring string_to_wstring(std::string_view in);

    enum class EVerifyReason { EMPTY, TOO_SHORT, INVALID_CHARACTERS, OK };

    constexpr auto MIN_NAME_LEN = 4;
    EVerifyReason verify_name(std::string_view in, size_t min_len = MIN_NAME_LEN);

    std::string demangle_name(std::string_view name);

    static const char *to_string(const EVerifyReason reason) {
        switch (reason) {
        case EVerifyReason::EMPTY:
            return "empty";
        case EVerifyReason::TOO_SHORT:
            return "too short";
        case EVerifyReason::INVALID_CHARACTERS:
            return "invalid characters (not alphanum or _)";
        case EVerifyReason::OK:
            return "ok";
        default:
            return "Unknown";
        }
    }

    template <typename V, typename... T>
    constexpr auto array_of(T &&...t) -> std::array<V, sizeof...(T)> {
        return {{std::forward<T>(t)...}};
    }
} // namespace Utils
