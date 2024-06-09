#include "ImGuiHelpers.h"

#include <Gui/Gui.h>
#include <Gui/IconsLoader/IconsLoader.h>

#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <Logger/Logger.h>

#include <Utils/ScopeGuard.h>

namespace ImGui::Helpers {
    bool title_bar_button(const std::string_view str_id, const ImTextureID texture_id, const ImVec2 &button_size,
                          const ImVec2 &image_offset, const ImVec2 &image_size, const float rounding,
                          const ImDrawFlags_ round_side, const bool imgui_size, const bool highlight) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        /*ImRect clip_rect;
        if (window->Flags & ImGuiWindowFlags_MenuBar) {
            clip_rect = window->MenuBarRect();
        }
        else if (!(window->Flags & ImGuiWindowFlags_NoTitleBar)) {
            clip_rect = window->TitleBarRect();
        }*/

        const auto id = window->GetID(str_id);
        const auto cursor_pos = window->DC.CursorPos;

        ImRect bb(cursor_pos, cursor_pos + button_size);

        if (imgui_size) {
            ItemSize(bb);
        }

        if (!ItemAdd(bb, id)) {
            return false;
        }

        // PushClipRect(clip_rect.Min, clip_rect.Max, false);
        // ScopeGuard clip_guard(&PopClipRect);

        // TODO: Find a better way of doing this, fixes the corner icon drawing outside the window when scrolled
        bool did_clip = false;
        if (rounding > 0.f) {
            auto &stack = window->DrawList->_ClipRectStack;
            if (!stack.empty()) {
                auto &last_clip_rect = stack.back();
                const ImRect new_clip_rect(last_clip_rect.x, last_clip_rect.y, last_clip_rect.z + rounding,
                                           last_clip_rect.w);
                PushClipRect(new_clip_rect.Min, new_clip_rect.Max, false);
                did_clip = true;
            }
        }

        bool hovered, held;
        const auto pressed = ButtonBehavior(bb, id, &hovered, &held, {});

        if (highlight) {
            hovered = true;
        }

        const auto col = GetColorU32(held && hovered ? ImGuiCol_ButtonActive
                                         : hovered   ? ImGuiCol_ButtonHovered
                                                     : ImGuiCol_Button);

        window->DrawList->AddRectFilled(bb.Min - ImVec2{0.f, 1.f}, bb.Max, col, rounding, round_side);
        bb.Translate(image_offset);

        if (image_size.x > 0.f || image_size.y > 0.f) {
            bb = ImRect(cursor_pos, cursor_pos + image_size);
            bb.Translate(image_offset);
        }

        window->DrawList->AddImage(texture_id, bb.Min, bb.Max, {0.f, 0.f}, {1.f, 1.f}, ImColor(255, 255, 255, 255));

        if (did_clip) {
            PopClipRect();
        }

        return hovered && pressed;
    }

    bool link_text(const std::string_view str) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        const auto id = window->GetID(str);

        const auto button_size = CalcTextSize(str);
        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + button_size);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) {
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, {});

        if (hovered) {
            SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        const auto col = GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive);

        window->DrawList->AddText(bb.Min, col, str);
        window->DrawList->AddLine({bb.Min.x, bb.Max.y}, {bb.Max.x, bb.Max.y}, col);
        return pressed;
    }

    bool input_text_top_label(const std::string_view label, std::string &str, ImGuiInputTextFlags flags,
                              const ImGuiInputTextCallback callback, void *user_data) {
        if (!label.starts_with("##")) {
            TextUnformatted(label);
        }

        SetNextItemWidth(-1.f);
        return InputText(fmt::format("##Label_{}", label), &str, flags, callback, user_data);
    }

    void draw_image(const std::string_view icon_name, const ImVec2 &size) {
        auto cursor_pos = GetCursorScreenPos();
        cursor_pos.x = std::floor(cursor_pos.x);
        cursor_pos.y = std::floor(cursor_pos.y);

        Dummy(size);
        GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(IconsLoader::get_icon_texture(
                                          icon_name, static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y))),
                                      cursor_pos, {cursor_pos.x + size.x, cursor_pos.y + size.y});
    }

    bool three_dot_button(const std::string_view str_id, const ImVec2 &button_size, float dot_size) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        const auto id = window->GetID(str_id);
        const auto scale_factor = Gui::the().scale_factor;

        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + button_size);
        ItemSize(bb);
        if (!ItemAdd(bb, id)) {
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, {});

        if (hovered) {
            SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        push_dark_style_button(true);
        window->DrawList->AddRectFilled(bb.Min, bb.Max,
                                        GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
        pop_dark_style_button(true);

        dot_size *= scale_factor;
        auto center = bb.GetCenter() - ImVec2(dot_size * 0.5f, dot_size * 0.5f);

        const auto dot_size_vec = ImVec2(dot_size, dot_size);

        center.y -= 4.f * scale_factor;
        window->DrawList->AddRectFilled(center, center + dot_size_vec, IM_COL32(255, 255, 255, 255));

        center.y += 4.f * scale_factor;
        window->DrawList->AddRectFilled(center, center + dot_size_vec, IM_COL32(255, 255, 255, 255));

        center.y += 4.f * scale_factor;
        window->DrawList->AddRectFilled(center, center + dot_size_vec, IM_COL32(255, 255, 255, 255));

        return pressed;
    }

    void draw_buttons_list(const ButtonsVecType &buttons, const bool apply_rounding) {
        const auto old_cursor_pos = GetCursorPos();

        const auto start_x = GetWindowWidth() - 1.f;
        auto x_offset = 0.f;

        push_dark_style_button();

        const auto scale_factor = Gui::the().scale_factor;
        for (const auto &button : buttons) {
            const auto button_size = button.size;

            x_offset += button_size.x;
            SetCursorPosX(start_x - x_offset + 1.f);

            if (title_bar_button(button.id,
                                 reinterpret_cast<ImTextureID>(IconsLoader::get_icon_texture(
                                     button.image, static_cast<uint32_t>(button.image_size.x * scale_factor),
                                     static_cast<uint32_t>(button.image_size.y * scale_factor))),
                                 button_size, button.offset, button.image_size * scale_factor,
                                 apply_rounding ? GUI_ROUNDING_RADIUS - 1.f : 0.f,
                                 apply_rounding ? ImDrawFlags_RoundCornersTopRight : ImDrawFlags_None,
                                 false, button.highlight)) {
                button.on_click();
            }

            if (!button.tooltip.empty() && BeginItemTooltip()) {
                TextUnformatted(button.tooltip);
                EndTooltip();
            }
        }

        pop_dark_style_button();
        SetCursorPos(old_cursor_pos);
    }

    void push_dark_style_button(const bool set_background_color) {
        if (set_background_color) {
            PushStyleColor(ImGuiCol_Button, 0);
        }

        PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(64, 64, 64, 128));
        PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(128, 128, 128, 128));
    }

    void pop_dark_style_button(const bool set_background_color) { PopStyleColor(set_background_color ? 3 : 2); }

    float get_char_width(const char c) {
        const auto *cur_font = GetFont();
        const auto *glyph = cur_font->FindGlyph(static_cast<ImWchar>(c));
        if (glyph == nullptr) {
            return 0.f;
        }

        const auto scale = GetCurrentContext()->FontSize / cur_font->FontSize;
        return glyph->AdvanceX * scale;
    }

    void colored_text_from_str(const std::string_view text, const uint32_t color) {
        PushStyleColor(ImGuiCol_Text, color);
        TextUnformatted(text);
        PopStyleColor();
    }

    void colored_text_from_str(const std::string_view text, const ImVec4 &color) {
        PushStyleColor(ImGuiCol_Text, color);
        TextUnformatted(text);
        PopStyleColor();
    }
} // namespace ImGui::Helpers
