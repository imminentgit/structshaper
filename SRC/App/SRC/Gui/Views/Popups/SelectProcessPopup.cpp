#include "SelectProcessPopup.h"

#include <ranges>

#include <Gui/Gui.h>

#include <ProcessInterface/ProcessInterface.h>

#include <Logger/Logger.h>
#include <Utils/ScopeGuard.h>

#include <Utils/expected_result.h>

SelectProcessPopup::SelectProcessPopup() : PopupView("Select a process") {
    size_behavior.HALF_MAIN_WINDOW_X = true;
    size_behavior.HALF_MAIN_WINDOW_Y = true;
}

void SelectProcessPopup::build_process_graph() {
    m_process_graph.clear();

    auto* const proc_iface = ProcessInterface::the().current_loaded_interface();
    m_processes = MUST(proc_iface->get_processes({}));

    for (const auto &[pid, entry] : m_processes) {
        auto parent_id = entry.parent_id;
        if (!m_processes.contains(parent_id)) {
            parent_id = NativeProcessEntry::INVALID_ID;
        }
        m_process_graph[parent_id].emplace_back(pid);
    }

    for (auto &children : m_process_graph | std::views::values) {
        std::ranges::sort(children, [&](const size_t a, const size_t b) {
            return m_processes.at(a).sequence_number < m_processes.at(b).sequence_number;
        });
    }
}

void SelectProcessPopup::draw_node(size_t pid, const size_t level) {
    if (!m_processes.contains(pid)) {
        return;
    }

    const auto &entry = m_processes.at(pid);
    const auto title = fmt::format("{} - PID: {}", entry.name, pid);
    const auto &children = m_process_graph[pid];

    const auto attach_process = [&] {
        if (auto res = ProcessInterface::the().current_process().attach(pid, entry.name)) {
            close_popup();
            Gui::the().add_notification("Attached to process", title);
        }
        else {
            Gui::the().add_notification("Failed to attach to the process", res.error());
        }
    };

    if (!m_search_text.empty()) {
        auto search_str = m_search_text;
        std::ranges::transform(search_str, search_str.begin(), ::tolower);

        bool found = false;
        for (const auto child_pid : children) {
            const auto &child_info = m_processes.at(child_pid);

            auto child_name = child_info.name;
            std::ranges::transform(child_name, child_name.begin(), ::tolower);

            if (child_name.contains(search_str)) {
                found = true;
                break;
            }
        }

        auto entry_name = entry.name;
        std::ranges::transform(entry_name, entry_name.begin(), ::tolower);

        if (!found && !entry_name.contains(search_str)) {
            return;
        }
    }

    if (children.empty()) {
        ImGui::Bullet();
        ImGui::SameLine();
        if (ImGui::Selectable(title)) {
            attach_process();
        }
    }
    else {
        if (ImGui::TreeNode(title)) {
            ImGui::Bullet();
            ImGui::SameLine();

            const auto select_parent_title = fmt::format("Select parent - PID: {}", pid);
            if (ImGui::Selectable(select_parent_title)) {
                attach_process();
            }

            for (const auto child_pid : m_process_graph[pid]) {
                draw_node(child_pid, level + 1);
            }
            ImGui::TreePop();
        }
    }
}

void SelectProcessPopup::draw_graph() {
    for (const auto root : m_process_graph[NativeProcessEntry::INVALID_ID]) {
        draw_node(root, NativeProcessEntry::INVALID_ID);
    }
}

void SelectProcessPopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();

    input_text_top_label("##ProcesFilter", m_search_text);

    try {
        if (m_process_graph.empty()) {
            build_process_graph();
        }

        ImGui::BeginChild("##ProcessList", {-FLT_MIN, -FLT_MIN},
                          ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
        ScopeGuard list_guard(&ImGui::EndChild);

        draw_graph();
    }
    catch (const std::exception &e) {
        ImGui::TextWrapped("Failed to get processes: %s", e.what());
    }
}

void SelectProcessPopup::on_buttons_add() {
    using namespace ImGui::Helpers;

    PopupView::on_buttons_add();

    Button refresh_project_list_button = {.id = "##RefreshProjectListButton",
                                          .image = "refresh",
                                          .tooltip = "Refresh the project list",
                                          .size = Gui::menubar_wh(),
                                          .image_size = {16.f, 16.f},
                                          .offset = {2.f, 2.f},
                                          .on_click = [this] { m_process_graph.clear(); }};
    buttons.emplace_back(refresh_project_list_button);
}
