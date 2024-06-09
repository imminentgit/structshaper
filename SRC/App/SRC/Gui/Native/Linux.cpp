#ifdef STRUCTSHAPER_IS_LINUX

#include <filesystem>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <backends/imgui_impl_glfw.h>

#include <imgui_internal.h>

#include <fontconfig/fontconfig.h>

#include <Gui/Gui.h>
#include <X11/Xlib.h>

#include <Logger/Logger.h>
#include <Utils/ScopeGuard.h>

bool get_default_font_path_for_pattern(FcConfig *config, const char *pattern, std::string& out_path) {
    auto *default_font_pattern = FcNameParse(reinterpret_cast<const FcChar8 *>(pattern));
    if (default_font_pattern == nullptr) {
        LOG_WARN("Failed to create default font pattern");
        return false;
    }

    ScopeGuard default_font_guard(&FcPatternDestroy, default_font_pattern);

    FcConfigSubstitute(config, default_font_pattern, FcMatchPattern);
    FcDefaultSubstitute(default_font_pattern);

    FcResult result;
    FcPattern *font = FcFontMatch(config, default_font_pattern, &result);
    if (!font) {
        LOG_WARN("Failed to get default font for pattern: {}", pattern);
        return false;
    }

    ScopeGuard font_guard(&FcPatternDestroy, font);

    FcChar8 *file{};
    if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
        LOG_WARN("Failed to get font file for pattern: {}", pattern);
        return false;
    }

    out_path = reinterpret_cast<const char *>(file);
    return true;
}

bool Gui::grab_fonts_via_fc() {
    auto *config = FcInitLoadConfigAndFonts();
    if (config == nullptr) {
        LOG_WARN("Failed to get current fontconfig config");
        return false;
    }

    ScopeGuard config_guard(&FcConfigDestroy, config);

    auto *pattern = FcPatternCreate();
    if (pattern == nullptr) {
        LOG_WARN("Failed to create fontconfig pattern");
        return false;
    }

    ScopeGuard pattern_guard(&FcPatternDestroy, pattern);

    auto *os = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_STYLE, FC_WEIGHT, FC_SPACING, FC_SLANT, nullptr);
    if (os == nullptr) {
        LOG_WARN("Failed to build fontconfig object set");
        return false;
    }

    ScopeGuard os_guard(&FcObjectSetDestroy, os);

    auto *font_list = FcFontList(config, pattern, os);
    if (font_list == nullptr) {
        LOG_WARN("Failed to get font list");
        return false;
    }

    ScopeGuard font_list_guard(&FcFontSetDestroy, font_list);

    for (int i = 0; i < font_list->nfont; i++) {
        const auto *font = font_list->fonts[i];

        FcChar8 *family{};
        if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) {
            LOG_WARN("Failed to get font family");
            continue;
        }

        FcChar8 *file{};
        if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
            LOG_WARN("Failed to get font file for family: {}", reinterpret_cast<const char *>(family));
            continue;
        }

        int spacing{};
        if (FcPatternGetInteger(font, FC_SPACING, 0, &spacing) != FcResultMatch) {
            spacing = FC_PROPORTIONAL;
        }

        FcChar8 *style{};;
        if (FcPatternGetString(font, FC_STYLE, 0, &style) != FcResultMatch) {
            LOG_WARN("Failed to get font style for family: {}", reinterpret_cast<const char *>(family));
        }

        int weight{};
        if (FcPatternGetInteger(font, FC_WEIGHT, 0, &weight) != FcResultMatch) {
            weight = 400;
        }

        FontEntry entry{};
        entry.recommended_weight = weight;

        if (style) {
            entry.type_face_name =
                fmt::format("{} ({})", reinterpret_cast<const char *>(family), reinterpret_cast<const char *>(style));
        }
        else {
            entry.type_face_name = reinterpret_cast<const char *>(family);
        }

        entry.file_path = reinterpret_cast<const char *>(file);
        entry.flags.is_monospace = spacing == FC_MONO;

        installed_fonts[entry.file_path] = entry;
    }

    std::string default_font_path{}, default_monospace_font_path{};
    if (!get_default_font_path_for_pattern(config, "sans-serif", default_font_path)) {
        LOG_WARN("Failed to get default font");
        return false;
    }
    default_font_entry = &installed_fonts.at(default_font_path);
    LOG_INFO("Default font name: {}", default_font_entry->type_face_name);

    if (!get_default_font_path_for_pattern(config, "monospace", default_monospace_font_path)) {
        LOG_WARN("Failed to get default monospace font");
        return false;
    }
    default_monospace_font_entry = &installed_fonts.at(default_monospace_font_path);
    LOG_INFO("Default monospace font name: {}", default_font_entry->type_face_name);

    return true;
}

void Gui::native_grab_fonts() {
    // Try fontconfig first:
    if (grab_fonts_via_fc()) {
        return;
    }

    LOG_WARN("Failed to grab fonts via fontconfig, trying to read from filesystem");
    for (auto &folder : {"/usr/share/fonts", "/usr/local/share/fonts", "/usr/share/fonts/truetype"}) {
        try {
            for (auto &entry : std::filesystem::directory_iterator(folder)) {
                if (entry.is_regular_file() && entry.path().extension() == ".ttf") {
                    auto font_name = entry.path().filename().string();
                    auto font_name_no_ext = font_name.substr(0, font_name.size() - 4);

                    installed_fonts[font_name_no_ext] = {400, font_name_no_ext};
                }
            }
        }
        catch (const std::exception &e) {
            LOG_WARN("Failed to read fonts from {}: {}", folder, e.what());
        }
    }
}

// void handle_x11_window_pos(const char* display_name) {
//     auto* display = XOpenDisplay(display_name);
//     if (display == nullptr) {
//         LOG_WARN("Failed to open X11 display: {}", display_name);
//         return;
//     }
//
//     ScopeGuard display_guard(&XCloseDisplay, display);
//
//     auto wnd = XDefaultRootWindow(display);
//
//     int monitor_count = 0;
//     auto *info = XRRGetMonitors(display, wnd, 0, &monitor_count);
//     ScopeGuard info_guard(&XRRFreeMonitors, info);
//
//     while (monitor_count > 0) {
//         monitor_count--;
//
//         if (info[monitor_count].primary) {
//             break;
//         }
//     }
// }

void Gui::native_init() const {
    if (!run_as_borderless) {
        return;
    }
}

void Gui::native_end() const {}

bool is_inside_rect(const int x, const int y, const int w, const int h) { return x >= 0 && x <= w && y >= 0 && y <= h; }

bool s_dragging = false, s_last_pressed = false;
int s_dragging_x = 0;
int s_dragging_y = 0;

void Gui::native_pre_render() const {
    if (!run_as_borderless) {
        return;
    }

    const auto *hovered_window = imgui_context->HoveredWindow;
    const std::string_view hovered_win = hovered_window != nullptr ? hovered_window->Name : "";

    double _cursor_x, _cursor_y;
    glfwGetCursorPos(glfw_window, &_cursor_x, &_cursor_y);

    const auto cursor_x = static_cast<int>(_cursor_x);
    const auto cursor_y = static_cast<int>(_cursor_y);

    if (hovered_win == GUI_MAIN_ID &&
        is_inside_rect(cursor_x, cursor_y, static_cast<int>(main_window_size.x),
                       static_cast<int>(ImGui::GetFrameHeight())) &&
        !s_dragging) {
        if (glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            s_dragging_x = cursor_x;
            s_dragging_y = cursor_y;
            s_dragging = true;
        }
    }

    if (s_dragging && glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        s_dragging = false;
    }

    if (s_dragging) {
        auto cursor_delta_x = static_cast<int>(cursor_x - s_dragging_x);
        auto cursor_delta_y = static_cast<int>(cursor_y - s_dragging_y);

        int window_x, window_y;
        glfwGetWindowPos(glfw_window, &window_x, &window_y);
        glfwSetWindowPos(glfw_window, window_x + cursor_delta_x, window_y + cursor_delta_y);
    }
}

void Gui::native_post_render() {}

#endif
