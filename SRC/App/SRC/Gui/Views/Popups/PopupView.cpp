#include "PopupView.h"

#include <Gui/Gui.h>

#include <Utils/ScopeGuard.h>

size_t PopupView::register_popup(PopupViewPtr &&view) {
    auto &vec = Gui::the().popups;
    const auto idx = vec.size();
    vec.emplace_back(view);
    return idx;
}

void PopupView::show_popup(size_t idx, const std::shared_ptr<void> &user_data) {
    auto &gui = Gui::the();

    gui.popup_stack.emplace(idx, user_data);
    ++gui.queued_popups;
}

bool PopupView::begin() {
    using namespace ImGui::Helpers;

    const auto display_size = Gui::the().main_window_size_with_bar_h();

    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar;

    if (horizontal_scrollbar) {
        flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    if (!scrolling) {
        flags |= ImGuiWindowFlags_NoScrollbar;
    }

    auto window_size = size;

    if (size_behavior.AUTO_SIZE_X) {
        window_size.x = -FLT_MIN;
    }

    if (size_behavior.AUTO_SIZE_Y) {
        window_size.y = -FLT_MIN;
    }

    if (size_behavior.HALF_MAIN_WINDOW_X) {
        window_size.x = display_size.x * 0.5f;
    }

    if (size_behavior.HALF_MAIN_WINDOW_Y) {
        window_size.y = display_size.y * 0.5f;
    }

    if (size_behavior.CUSTOM_X) {
        window_size.x = size.x;
    }

    if (size_behavior.CUSTOM_Y) {
        window_size.y = size.y;
    }

    ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
    ImGui::SetNextWindowPos({display_size.x * 0.5f, display_size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    return ImGui::Begin(title, nullptr, flags);
}

void PopupView::render() { BaseView::render(); }

void PopupView::end() { ImGui::End(); }

void PopupView::on_buttons_add() {
    using namespace ImGui::Helpers;
    const auto buttons_size = Gui::menubar_wh();
    Button close_button = {.id = "##CloseButtonPopup",
                           .image = "close",
                           .tooltip = "Close",
                           .size = buttons_size,
                           .image_size = {20.f, 20.f},
                           .on_click = [this] { close_popup(); }};

    buttons.emplace_back(close_button);
}
