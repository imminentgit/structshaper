#include "SelectProcessInterfacePopup.h"

#include <Gui/Gui.h>

#include <ProcessInterface/ProcessInterface.h>

#include <Utils/ScopeGuard.h>
#include <Utils/expected_result.h>

SelectProcessInterfacePopup::SelectProcessInterfacePopup() : PopupView("Select a new process interface") {
    size_behavior.HALF_MAIN_WINDOW_X = true;
    size_behavior.HALF_MAIN_WINDOW_Y = true;
}

void SelectProcessInterfacePopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();

    const auto interfaces = ProcessInterface::grab_interfaces();
    if (interfaces.empty()) {
        aligned_win_size(CENTER_BOTH, [] { ImGui::TextUnformatted("No memory interfaces found."); });
        return;
    }

    ImGui::BeginChild("##MemoryInterfaceList", {-FLT_MIN, -FLT_MIN},
                      ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
    ScopeGuard list_guard(&ImGui::EndChild);

    for (const auto &interface_entry : interfaces) {
        if (ImGui::Selectable(interface_entry.filename)) {
            try {
                auto &current_project = ProjectManager::the().project_info;
                MUST(ProcessInterface::the().load_process_interface_from_path(interface_entry.path));

                current_project.process_interface_path = interface_entry.path;
                close_popup();
            }
            catch (const std::exception &e) {
                Gui::the().add_notification("Failed to load process interface", e.what());
            }
        }
    }
}
