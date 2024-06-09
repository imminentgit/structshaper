#pragma once

#include <expected>
#include <stdexcept>

#include <fmt/format.h>
#include <fmt/os.h>

#define TRY(expr)                                                                                                      \
    ({                                                                                                                 \
        auto res = (expr);                                                                                             \
        if (!res) {                                                                                                    \
            return std::unexpected(res.error());                                                                       \
        }                                                                                                              \
        *res;                                                                                                          \
    })

#define MUST(expr)                                                                                                     \
    ({                                                                                                                 \
        auto res = (expr);                                                                                             \
        if (!res) {                                                                                                    \
            throw std::runtime_error(res.error());                                                                     \
        }                                                                                                              \
        *res;                                                                                                          \
    })

template <typename... Args>
auto unexpected_fmt(const std::string_view message, const Args &...args) {
    return std::unexpected(fmt::vformat(message, fmt::make_format_args(args...)));
}

template <typename... Args>
auto unexpected_ec(const int error_code, const std::string_view message, const Args &...args) {
    const auto ec = std::error_code(error_code, fmt::system_category());
    return std::unexpected(fmt::format("{}: {}", ec.message(), fmt::vformat(message, fmt::make_format_args(args...))));
}
