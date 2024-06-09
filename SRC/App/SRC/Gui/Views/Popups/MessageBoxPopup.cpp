#include "MessageBoxPopup.h"

#include <Gui/Gui.h>
#include <Gui/ImGuiHelpers/ImGuiHelpers.h>

MessageBoxPopup::MessageBoxPopup() : PopupView("MessageBox_RENAME_ME") {
    size_behavior.AUTO_SIZE_X = true;
    size_behavior.AUTO_SIZE_Y = true;
    scrolling = false;
}

void MessageBoxPopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();

    auto msg = std::static_pointer_cast<MessageBoxData>(user_data);
    title = msg->title;

    const auto win_size_with_bar = ImGui::GetWindowSize() + ImVec2(0.f, ImGui::GetFrameHeight());

    ImGui::BeginGroup();

    draw_image("question", {32.f, 32.f});
    ImGui::SameLine();

    const auto same_line_cursor = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(same_line_cursor + 8.f);
    ImGui::TextUnformatted(msg->message);
    ImGui::SetCursorPosY(same_line_cursor);

    ImGui::EndGroup();

    aligned(RIGHT, win_size_with_bar, [this, msg] {
        ImGui::BeginGroup();

        if (ImGui::Button(msg->no_text)) {
            if (msg->cancel_callback) {
                msg->cancel_callback();
            }

            close_popup();
        }

        ImGui::SameLine();
        if (ImGui::Button(msg->yes_text)) {
            if (msg->ok_callback) {
                msg->ok_callback();
            }

            close_popup();
        }

        ImGui::EndGroup();
    });
}

bool MessageBoxPopup::on_close(const bool after_fade) {
    const auto msg = std::static_pointer_cast<MessageBoxData>(user_data);
    if (msg->cancel_callback) {
        msg->cancel_callback();
    }
    return true;
}
