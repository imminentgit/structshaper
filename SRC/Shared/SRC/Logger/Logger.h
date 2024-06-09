#pragma once

#include <source_location>
#include <list>
#include <stack>

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <fmt/os.h>

namespace Logger {
    namespace detail {
        static void print_time() {
            fmt::print(fmt::fg(fmt::color::slate_gray), "[");
            fmt::print("{:%H:%M:%S}", fmt::localtime(std::time(nullptr)));
            fmt::print(fmt::fg(fmt::color::slate_gray), "] ");
        }

        static void put_prefix(const std::string_view prefix, const fmt::color prefix_color) {
            fmt::print(fmt::fg(fmt::color::slate_gray), "[");
            fmt::print(fmt::fg(prefix_color), "{}", prefix);
            fmt::print(fmt::fg(fmt::color::slate_gray), "] ");
        }

        static void put_source_location(const uint32_t column, const std::string_view function) {
            fmt::print(fmt::fg(fmt::color::slate_gray), "[");
            fmt::print("{}:{}", function, column);
            fmt::print(fmt::fg(fmt::color::slate_gray), "] ");
        }
    }

#define DO_LOG_IMPL(func_name, prefix, prefix_color) \
    template<typename... TArgs> \
    static void func_name(const uint32_t column, const std::string_view function, const std::string_view str, \
              TArgs &&... args) { \
    \
        detail::put_prefix(prefix, prefix_color); \
        detail::print_time(); \
        detail::put_source_location(column, function); \
    \
        fmt::vprint(str, fmt::make_format_args(args...)); \
        fmt::print("\n"); \
    }

    DO_LOG_IMPL(msg, "LOG", fmt::color::white);

    DO_LOG_IMPL(info, "INFO", fmt::color::green_yellow);

    DO_LOG_IMPL(warn, "WARN", fmt::color::yellow);

    DO_LOG_IMPL(error, "ERROR", fmt::color::orange_red);
}

#ifdef _DEBUG
#define _LOG_DEBUG_REFERENCE __builtin_FILE()
#else
#define _LOG_DEBUG_REFERENCE __builtin_FUNCTION()
#endif

#define LOG_MSG(_msg, ...) Logger::msg(__builtin_LINE(), _LOG_DEBUG_REFERENCE, _msg __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO(msg, ...) Logger::info(__builtin_LINE(), _LOG_DEBUG_REFERENCE, msg __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARN(msg, ...) Logger::warn(__builtin_LINE(), _LOG_DEBUG_REFERENCE, msg __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(msg, ...) Logger::error(__builtin_LINE(), _LOG_DEBUG_REFERENCE, msg __VA_OPT__(,) __VA_ARGS__)

template<typename... Args>
std::runtime_error formatted_error(const std::string_view message,
                                   const Args &... args) {
    return std::runtime_error(fmt::vformat(message, fmt::make_format_args(args...)));
}

template <typename T, typename FormatContext>
static auto format_t_vec_type(const char* prefix, const T &t, FormatContext &ctx) {
    if (t.empty()) {
        return fmt::format_to(ctx.out(), "{}[size: {}, capacity: {}]", prefix, t.size(), t.capacity());
    }

    std::string tmp;
    size_t i = 0;
    for (const auto &elem : t) {
        tmp += fmt::format("{}", elem);
        if (i != t.size() - 1) {
            tmp += ", ";
        }
        ++i;
    }

    return fmt::format_to(ctx.out(), "{}[size: {}, capacity: {}, data: {}]", prefix, t.size(), t.capacity(), tmp);
}

template<typename T>
struct fmt::formatter<std::vector<T>> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const std::vector<T> &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
       return format_t_vec_type<std::vector<T>, FormatContext>("vec", vec, ctx);
    }
};

template<typename T>
struct fmt::formatter<std::stack<T>> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const std::vector<T> &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return format_t_vec_type<std::vector<T>, FormatContext>("stack", vec, ctx);
    }
};

template<typename T>
struct fmt::formatter<std::list<T>> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const std::vector<T> &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return format_t_vec_type<std::vector<T>, FormatContext>("list", vec, ctx);
    }
};

template<typename T>
struct fmt::formatter<std::deque<T>> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const std::vector<T> &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return format_t_vec_type<std::vector<T>, FormatContext>("deque", vec, ctx);
    }
};