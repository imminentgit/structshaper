#include "CreateProjectPopup.h"

#include <Gui/Gui.h>

#include <ProcessInterface/ProcessInterface.h>

CreateProjectPopup::CreateProjectPopup() : PopupView("Create a new Project") {
    size_behavior.HALF_MAIN_WINDOW_X = true;
    size_behavior.AUTO_SIZE_Y = true;
}

void CreateProjectPopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();

    try {
        auto interfaces = ProcessInterface::grab_interfaces();

        if (interfaces.empty()) {
            aligned(CENTER_BOTH, ImGui::GetWindowSize(), [] { ImGui::TextUnformatted("No memory interfaces found."); });
            return;
        }

        input_text_top_label("Name:", m_new_project_info.name);
        const auto name_valid = Utils::verify_name(m_new_project_info.name);
        const auto can_not_create_project = name_valid != Utils::EVerifyReason::OK;
        if (name_valid != Utils::EVerifyReason::OK) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(238, 75, 43, 204));
            ImGui::Text(name_valid == Utils::EVerifyReason::INVALID_CHARACTERS ? "Name has %s:" : "Name is %s",
                        to_string(name_valid));
            ImGui::PopStyleColor();
        }

        input_text_top_label("Description:", m_new_project_info.description);

        ImGui::TextUnformatted("Memory Interface:");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::Combo(
            "##MemoryInterface_Combo", &m_selected_interface,
            [](void *data, const int idx) -> ImStrv {
                const auto &interface_entry = *static_cast<ProcessInterface::InterfaceVec *>(data);
                return interface_entry[idx].filename.data();
            },
            &interfaces, static_cast<int>(interfaces.size()));

        if (aligned_win_size(RIGHT, [can_not_create_project] {
                ImGui::BeginDisabled(can_not_create_project);
                const auto res =
                    ImGui::Button("Create") || !can_not_create_project && ImGui::IsKeyPressed(ImGuiKey_Enter);
                ImGui::EndDisabled();
                return res;
            })) {

            m_new_project_info.process_interface_path = interfaces[m_selected_interface].path;

            try {
                ProjectManager::the().create_project(m_new_project_info);
                close_popup();
            }
            catch (const std::exception &e) {
                Gui::the().add_notification("Failed to create project", e.what());
            }
        }
    }
    catch (const std::exception &e) {
        ImGui::TextUnformatted("Failed to iterate memory interfaces:");
        ImGui::TextWrapped("%s", e.what());
    }
}

bool CreateProjectPopup::on_close(const bool after_fade) {
    if (after_fade) {
        m_new_project_info = {};
    }
    return true;
}
