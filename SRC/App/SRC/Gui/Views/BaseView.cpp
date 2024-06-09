#include "BaseView.h"

#include <ranges>

#include <Gui/Gui.h>

#include <Logger/Logger.h>

void BaseView::close_views(const EViewType type) {
    auto &views = Gui::the().views;
    for (const auto &view : views | std::views::values) {
        if (view->view_type == type) {
            view->should_render = false;
        }
    }
}

void BaseView::remove_view(const UniversalId id) {
    auto &gui = Gui::the();

    gui.views.erase(id);
    gui.view_id_allocator.free(id);

    LOG_INFO("Removed view with id: {}", id);
}

UniversalId BaseView::register_view(BaseViewPtr &&view) {
    auto &gui = Gui::the();

    const auto id = gui.view_id_allocator.allocate();

    view->id = id;
    gui.views.insert_or_assign(id, std::move(view));
    return id;
}

void BaseView::show_view(const UniversalId id, const bool show) {
    const auto &gui = Gui::the().views;
    auto &view = gui.at(id);
    view->should_render = show;

    // TODO: Maybe defer this?
    if (show) {
        view->on_show();
    }
}

bool BaseView::begin() {
    begin_docked_callback();
    const auto res = ImGui::Begin(title, nullptr);

    return res;
}

void BaseView::render() {
    using namespace ImGui::Helpers;

    if (is_docked) {
        end_docked_callback();
        did_push = false;
    }
    is_docked = ImGui::IsWindowDocked();

    if (!has_added_buttons) {
        on_buttons_add();
        has_added_buttons = true;
    }

    if (ImGui::BeginMenuBar()) {
        draw_buttons_list(buttons, true);

        ImGui::TextUnformatted(title);
        ImGui::EndMenuBar();
    }
}

void BaseView::end() {
    end_docked_callback();
    ImGui::End();
}

bool BaseBackgroundView::begin() {
    const auto win_size =
        !invisible_covering_window ? Gui::the().main_window_size_with_bar_h() : Gui::the().main_window_size;

    auto half_window_width = !invisible_covering_window ? win_size.x * 0.5f : win_size.x;
    auto half_window_height = !invisible_covering_window ? win_size.y * 0.5f : win_size.y;

    const auto centered_pos = !invisible_covering_window
        ? ImVec2(half_window_width - half_window_width * 0.5f, half_window_height - half_window_height * 0.5f)
        : ImVec2();

    ImGui::SetNextWindowPos(centered_pos);
    ImGui::SetNextWindowSize({half_window_width, half_window_height});
    const auto open = ImGui::BeginChild(title, {half_window_width, half_window_height}, ImGuiChildFlags_Border,
                                        ImGuiWindowFlags_MenuBar);

    if (!open) {
        return open;
    }

    return open;
}

void BaseBackgroundView::end() { ImGui::EndChild(); }
