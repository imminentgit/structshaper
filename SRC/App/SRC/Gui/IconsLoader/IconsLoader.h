#pragma once
#include <string>

#include <imgui.h>

#include <GLFW/glfw3.h>

#include <lunasvg.h>

#include <Defines.h>

namespace IconsLoader {
    using LunaDocument = std::unique_ptr<lunasvg::Document>;

    void init();

    LunaDocument::pointer get_icon(std::string_view icon_name);
    GLuint get_icon_texture(std::string_view icon_name, uint32_t width, uint32_t height);
}

struct Icon {
    const char* name;

    ALWAYS_INLINE Icon(const char* name) : name(name) {}

    ALWAYS_INLINE ImTextureID operator()(const size_t w, const size_t h) const {
        return reinterpret_cast<ImTextureID>(IconsLoader::get_icon_texture(name, w, h));
    }

    ALWAYS_INLINE ImTextureID operator()(const size_t size) const {
        return operator()(size, size);
    }

    ALWAYS_INLINE ImTextureID operator()(const ImVec2& size) const {
        return operator()(static_cast<size_t>(size.x), static_cast<size_t>(size.y));
    }
};

[[nodiscard]] ALWAYS_INLINE Icon operator""_svg_icon(const char* icon_name, size_t) {
    return {icon_name};
}