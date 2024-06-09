#pragma once

#include <cmath>
#include <functional>
#include <source_location>
#include <string>
#include <type_traits>
#include <vector>

#include <imgui.h>

#include <Utils/Fnv1.h>

namespace ImGui::Helpers {
    enum EAlignMode { CENTER_HORIZONTAL, CENTER_VERTICAL, CENTER_BOTH, RIGHT, BOTTOM_RIGHT };

    // TODO: Revamp this, maybe pass the args as a struct?
    bool title_bar_button(std::string_view str_id, ImTextureID texture_id, const ImVec2 &button_size,
                          const ImVec2 &image_offset = {}, const ImVec2 &image_size = {}, float rounding = 0.f,
                          ImDrawFlags_ round_side = ImDrawFlags_None, bool imgui_size = false, const bool highlight = false);

    bool link_text(std::string_view str);

    bool input_text_top_label(std::string_view label, std::string &str, ImGuiInputTextFlags flags = 0,
                              ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);

    void draw_image(std::string_view icon_name, const ImVec2 &size);

    bool three_dot_button(std::string_view str_id, const ImVec2 &button_size, float dot_size = 2.f);

    struct Button {
        std::string id{};
        std::string image{};
        std::string tooltip{};
        ImVec2 size{};
        ImVec2 image_size{};
        ImVec2 offset{};
        bool highlight{};
        std::function<void()> on_click{};
    };

    using ButtonsVecType = std::vector<Button>;

    void draw_buttons_list(const ButtonsVecType &buttons, bool apply_rounding = false);

    float get_char_width(char c);

    void colored_text_from_str(std::string_view text, uint32_t color);

    void colored_text_from_str(std::string_view text, const ImVec4& color);

    // TODO: Why is this true by default?
    void push_dark_style_button(bool set_background_color = true);

    void pop_dark_style_button(bool set_background_color = true);

    class ScopedCursorPos {
        ImVec2 m_cursor_pos{};

    public:
        ScopedCursorPos(const ImVec2 &cursor_pos) : m_cursor_pos(GetCursorPos()) { SetCursorPos(cursor_pos); }

        ~ScopedCursorPos() { SetCursorPos(m_cursor_pos); }
    };

    inline std::unordered_map<Utils::Fnv1::HashType, ImVec2> s_id_to_ctrl_size{};
    template <typename TImGuiFunction, typename... TArgs>
    constexpr auto aligned(const EAlignMode mode, ImVec2 window_size, TImGuiFunction &&callback,
                           const std::string_view id_addition = {},
                           const std::source_location &loc = std::source_location::current()) {
        window_size.x = std::ceil(window_size.x);
        window_size.y = std::ceil(window_size.y);

        auto id_hash = Utils::Fnv1::hash(loc.file_name()) ^ Utils::Fnv1::hash(loc.function_name()) ^ loc.line();
        if (!id_addition.empty()) {
            id_hash ^= Utils::Fnv1::hash(id_addition);
        }

        if (!s_id_to_ctrl_size.contains(id_hash)) {
            ScopedCursorPos scp({-99999.f, -99999.f});
            callback();

            auto control_size = GetItemRectSize();
            control_size.x = std::ceil(control_size.x);
            control_size.y = std::ceil(control_size.y);

            s_id_to_ctrl_size.emplace(id_hash, control_size);
        }

        auto cursor_pos = GetCursorPos();
        cursor_pos.x = std::ceil(cursor_pos.x);
        cursor_pos.y = std::ceil(cursor_pos.y);
        const auto CONTROL_PADDING_HORIZONTAL = GetStyle().ItemSpacing.x + 1.f;

        auto &control_size = s_id_to_ctrl_size.at(id_hash);
        switch (mode) {
        case CENTER_HORIZONTAL:
            SetCursorPos({(window_size.x - control_size.x) * 0.5f, cursor_pos.y});
            break;
        case CENTER_VERTICAL:
            SetCursorPos({cursor_pos.x, (window_size.y - control_size.y) * 0.5f});
            break;
        case CENTER_BOTH:
            SetCursorPos({(window_size.x - control_size.x) * 0.5f, (window_size.y - control_size.y) * 0.5f});
            break;
        case RIGHT:
            SetCursorPos({window_size.x - (control_size.x + CONTROL_PADDING_HORIZONTAL), cursor_pos.y});
            break;
        case BOTTOM_RIGHT:
            SetCursorPos({window_size.x - (control_size.x + CONTROL_PADDING_HORIZONTAL),
                          window_size.y - control_size.y - GetStyle().WindowPadding.y});
            break;
        }

        if constexpr (!std::is_same_v<std::remove_all_extents_t<std::invoke_result_t<TImGuiFunction, TArgs...>>,
                                      void>) {
            const auto res = callback();
            control_size = GetItemRectSize();
            control_size.x = std::ceil(control_size.x);
            control_size.y = std::ceil(control_size.y);
            return res;
        }
        else {
            callback();
            control_size = GetItemRectSize();
            control_size.x = std::ceil(control_size.x);
            control_size.y = std::ceil(control_size.y);
            return true;
        }
    }

    template <typename TImGuiFunction>
    constexpr auto aligned_avail(const EAlignMode mode, TImGuiFunction &&callback,
                                 const std::string_view id_addition = {},
                                 const std::source_location &loc = std::source_location::current()) {
        return aligned(mode, GetContentRegionAvail(), callback, id_addition, loc);
    }

    template <typename TImGuiFunction>
    constexpr auto aligned_win_size(const EAlignMode mode, TImGuiFunction &&callback,
                                    const std::string_view id_addition = {},
                                    const std::source_location &loc = std::source_location::current()) {
        return aligned(mode, GetWindowSize(), callback, id_addition, loc);
    }
} // namespace ImGui::Helpers
