#pragma once
#include <cstdint>

namespace Utils::Fnv1 {
#ifdef STRUCTSHAPER_IS_64_BIT
    using HashType = uint64_t;
    inline constexpr auto DEFAULT_OFFSET_BASIS = 0xcbf29ce484222325;
    inline constexpr auto PRIME = 0x100000001b3;
#else
    using HashType = uint32_t;
    inline constexpr auto DEFAULT_OFFSET_BASIS = 0x811C9DC5;
    inline constexpr auto PRIME = 0x01000193;
#endif

    constexpr HashType hash(const char *str, const HashType val = DEFAULT_OFFSET_BASIS) {
        return str[0ull] == '\0' ? val : hash(&str[1ull], (val ^ static_cast<HashType>(str[0ull])) * PRIME);
    }

    constexpr HashType hash(const wchar_t *str, const HashType val = DEFAULT_OFFSET_BASIS) {
        return str[0ull] == '\0' ? val : hash(&str[1ull], (val ^ static_cast<HashType>(str[0ull])) * PRIME);
    }

    constexpr HashType hash(const std::string_view str, const HashType val = DEFAULT_OFFSET_BASIS) {
        return hash(str.data(), val);
    }

    constexpr HashType hash(const std::wstring_view str, const HashType val = DEFAULT_OFFSET_BASIS) {
        return hash(str.data(), val);
    }
} // namespace Utils::Fnv1

#define FNV1(str) []() { return Utils::Fnv1::hash(str); }()
