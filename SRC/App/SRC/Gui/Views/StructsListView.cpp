#include "StructsListView.h"

#include "Popups/CreateStructPopup.h"

#include <Gui/Gui.h>

#include <misc/cpp/imgui_stdlib.h>

#include <ProjectManager/ProjectManager.h>

#include <Queue/Event.h>

#include <Logger/Logger.h>

#include <Utils/ScopeGuard.h>

#include <Utils/Utils.h>

StructsListView::StructsListView() : BaseView("Structs", EViewType::MAIN_VIEW) {
    should_render = false;

    Event::OnPreDraw::listen([](Gui &gui) {
        Event::StructRename::update_pull_single([](const std::string &old_name, const std::string &new_name) {
            auto &proj_manager = ProjectManager::the();
            try {
                proj_manager.rename_struct(old_name, new_name);
            }
            catch (const std::exception &e) {
                Gui::the().add_notification(fmt::format("Failed to rename {}:", old_name), e.what());
                return false;
            }
            return true;
        });

        Event::StructDelete::update_pull_single([](const std::string &name) {
            auto &proj_manager = ProjectManager::the();
            try {
                proj_manager.delete_struct(name);
            }
            catch (const std::exception &e) {
                Gui::the().add_notification(fmt::format("Failed to delete {}:", name), e.what());
                return false;
            }
            return true;
        });
    });
}

bool StructsListView::begin() {
    const auto &gui = Gui::the();
    ImGui::SetNextWindowDockID(gui.dockspace_left_id, ImGuiCond_Once);
    return BaseView::begin();
}

void StructsListView::render() {
    using namespace ImGui::Helpers;
    BaseView::render();

    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 25.f - ImGui::GetStyle().FramePadding.x * 4.f);
    ImGui::InputText("##StructSearchQuery", &m_search_query);

    std::ranges::transform(m_search_query, m_search_query.begin(), ::tolower);

    ImGui::SameLine();
    if (ImGui::Button("+")) {
        PopupView::show_popup(popup_CreateStructPopup);
    }

    ImGui::Separator();

    auto &proj_manager = ProjectManager::the();
    Event::MoveStruct::update_pull_single(
        [&proj_manager](const std::string &current_struct, UniversalId from, UniversalId to) {
            auto &structs = proj_manager.structs;
            const auto it = structs.find(current_struct);
            if (it == structs.end()) {
                LOG_ERROR("Failed to find struct {}", current_struct);
                return false;
            }

            auto &struct_ = it->second;

            const auto from_it = struct_.obtain_field_by_id(from);

            if (from_it == struct_.end_fields()) {
                LOG_ERROR("Failed to find field with id {} in struct", from, current_struct);
                return false;
            }

            const auto to_it = struct_.obtain_field_by_id(to);
            if (to_it == struct_.end_fields()) {
                LOG_ERROR("Failed to find field with id {} in struct", to, current_struct);
                return false;
            }

            struct_.splice_field_to_pos(from_it, to_it);
            LOG_INFO("Moved field {} to position {}", from, to);
            return true;
        });

    for (auto &[name, struct_] : proj_manager.structs) {
        const auto struct_window = struct_.obtain_struct_view();

        if (!m_search_query.empty()) {
            auto name_lower = name;
            std::ranges::transform(name_lower, name_lower.begin(), ::tolower);
            if (!name_lower.contains(m_search_query)) {
                continue;
            }
        }

        if (struct_.is_in_edit_mode) {
            ImGui::SetNextItemWidth(-1.f);
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText(make_imgui_id("_StructNameInput_",name), &struct_.tmp_new_name,
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (const auto reason = Utils::verify_name(struct_.tmp_new_name, 1);
                    reason == Utils::EVerifyReason::OK) {
                    Event::StructRename::call(name, struct_.tmp_new_name);
                    struct_.tmp_new_name.clear();
                    struct_.is_in_edit_mode = false;
                }
                else {
                    Gui::the().add_notification("Failed to rename struct", to_string(reason));
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                struct_.tmp_new_name.clear();
                struct_.is_in_edit_mode = false;
            }
        }
        else {
            if (ImGui::Selectable(name, struct_window->should_render)) {
                show_view(struct_.ui_view_id, !struct_window->should_render);
            }
        }

        if (ImGui::BeginPopupContextItem()) {
            ScopeGuard popup_guard(&ImGui::EndPopup);

            if (ImGui::MenuItem("Delete")) {
                Event::StructDelete::call(name);
            }

            if (ImGui::MenuItem("Rename")) {
                struct_.is_in_edit_mode = true;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Clear")) {
                struct_.clear_fields();
                break;
            }
        }
    }
}

bool StructsListView::begin_docked_callback_inner() {
    if (is_docked) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
        return true;
    }

    return false;
}

void StructsListView::end_docked_callback_inner() { ImGui::PopStyleColor(); }
