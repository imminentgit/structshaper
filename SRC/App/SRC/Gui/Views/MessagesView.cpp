#include "MessagesView.h"

#include <Gui/Gui.h>

#include <Logger/Logger.h>

struct MessageEntry {
    EMessageType type{};
    std::chrono::system_clock::time_point insertion_time{};
};

static std::unordered_map<std::string, MessageEntry> s_messages{};

MessagesView::MessagesView(): BaseView("Messages", EViewType::MAIN_VIEW) { should_render = false; }

bool MessagesView::begin() {
    const auto &gui = Gui::the();
    ImGui::SetNextWindowDockID(gui.dockspace_bottom_id, ImGuiCond_Once);
    return BaseView::begin();
}

void MessagesView::render() {
    using namespace ImGui::Helpers;

    BaseView::render();

    if (!ImGui::BeginTable(make_imgui_id("_MessageTable"), 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        return;
    }

    for (auto it = s_messages.begin(); it != s_messages.end();) {
        const auto& [message, entry] = *it;

        const auto time_since_insertion = std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::system_clock::now() - entry.insertion_time)
                                            .count();

        if (time_since_insertion > 1) {
            it = s_messages.erase(it);
            continue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const auto height = ImGui::GetTextLineHeight();

        std::string_view icon{};
        switch (entry.type) {
            case EMessageType::ERROR_MSG:
                icon = "error";
                break;
            case EMessageType::WARNING_MSG:
                icon = "warning";
                break;
            case EMessageType::INFO_MSG:
                icon = "info";
                break;
        }

        draw_image(icon, {height, height});
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(message.c_str());

        if (ImGui::BeginPopupContextItem(make_imgui_id("_{}_ContextMenu", message))) {
            if (ImGui::MenuItem("Copy message")) {
                ImGui::SetClipboardText(message);
                Gui::the().add_notification({}, "Copied message to clipboard");
            }

            ImGui::EndPopup();
        }

        ++it;
    }

    ImGui::EndTable();
}

void MessagesView::add(std::string message, const EMessageType type, const char* function_name, const int line) {
    MessageEntry entry{};
    entry.type = type;
    entry.insertion_time = std::chrono::system_clock::now();

    message = fmt::format("{}:{}: {}", function_name, line, message);

    if (!s_messages.contains(message)) {
        switch (type) {
            case EMessageType::ERROR_MSG:
                LOG_ERROR("{}", message);
                break;
            case EMessageType::WARNING_MSG:
                LOG_WARN("{}", message);
                break;
            case EMessageType::INFO_MSG:
                LOG_INFO("{}", message);
                break;
        }
    }

    s_messages[message] = entry;
}
