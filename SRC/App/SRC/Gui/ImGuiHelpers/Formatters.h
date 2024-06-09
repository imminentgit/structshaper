#pragma once
#include <imgui_internal.h>

template<>
struct fmt::formatter<ImVec2> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const ImVec2 &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "ImVec2({}, {})", vec.x, vec.y);
    }
};

template<>
struct fmt::formatter<ImVec4> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const ImVec4 &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "ImVec4({}, {}, {}, {})", vec.x, vec.y, vec.z, vec.w);
    }
};

template<>
struct fmt::formatter<ImRect> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const ImRect &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "ImRect({}, {})", vec.Min, vec.Max);
    }
};

template<>
struct fmt::formatter<ImColor> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template<typename FormatContext>
    auto format(const ImColor &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "ImColor({}, {}, {}, {}) -> RGBA({}, {}, {}, {})", vec.Value.x, vec.Value.y,
                              vec.Value.z * 255.f, vec.Value.w * 255.f, vec.Value.x * 255.f, vec.Value.y * 255.f,
                              vec.Value.z * 255.f, vec.Value.w * 255.f);
    }
};