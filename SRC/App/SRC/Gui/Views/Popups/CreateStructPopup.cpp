#include "CreateStructPopup.h"

#include <Gui/Gui.h>

#include <Queue/Event.h>

#include <Utils/Utils.h>

CreateStructPopup::CreateStructPopup() : PopupView("Create a new struct") {
    size_behavior.CUSTOM_X = true;
    size.x = 250.f;
    size_behavior.AUTO_SIZE_Y = true;
}

void CreateStructPopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();
    input_text_top_label("Name:", m_name);

    const auto name_valid = Utils::verify_name(m_name, 1);
    const auto can_not_create_struct = name_valid != Utils::EVerifyReason::OK;

    if (can_not_create_struct) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(238, 75, 43, 204));
        ImGui::Text(name_valid == Utils::EVerifyReason::INVALID_CHARACTERS ? "Name has %s:" : "Name is %s",
                    to_string(name_valid));
        ImGui::PopStyleColor();

        ImGui::SameLine();
    }

    if (aligned_win_size(RIGHT, [can_not_create_struct] {
            ImGui::BeginDisabled(can_not_create_struct);
            const auto res = ImGui::Button("Create") || !can_not_create_struct && ImGui::IsKeyPressed(ImGuiKey_Enter);
            ImGui::EndDisabled();
            return res;
        })) {
        try {
            Event::StructCreated::call(m_name);
            close_popup();
        }
        catch (const std::exception &e) {
            Gui::the().add_notification("Failed to create struct", e.what());
        }
    }
}
