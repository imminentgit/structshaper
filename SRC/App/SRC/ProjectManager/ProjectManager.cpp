#include "ProjectManager.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include <Gui/Gui.h>

#include <Gui/Views/MessagesView.h>
#include <Gui/Views/ProjectBackgroundView.h>
#include <Gui/Views/ProjectView.h>
#include <Gui/Views/StructsListView.h>
#include <Gui/Views/Popups/MessageBoxPopup.h>

#include <OptionsManager/OptionsManager.h>

#include <Logger/Logger.h>
#include <Queue/Event.h>

ProjectManager::ProjectManager() {
    Event::ProjectLoaded::listen([this](const ProjectInfo &project_info) {
        LOG_INFO("Project loaded: {}", project_info.name);

        Gui::the().set_window_title(fmt::format("structshaper: {}", project_info.name));

        BaseView::show_view(view_ProjectView, false);

        const auto projects_empty = structs.empty();
        BaseView::show_view(view_ProjectBackgroundView, projects_empty);
        BaseView::show_view(view_StructsListView, !projects_empty);

        BaseView::show_view(view_MessagesView);
    });

    Event::StructCreated::listen([this](const std::string &name) {
        LOG_INFO("Creating struct: {}", name);

        create_struct(name);

        // Is it the first struct created, then show the main structs view
        if (!structs.empty()) {
            BaseView::show_view(view_ProjectBackgroundView, false);
            BaseView::show_view(view_StructsListView, true);
        }
    });

    Event::OnMenuBarRender::listen(
        [this] {
            if (!is_project_loaded()) {
                return;
            }

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save")) {
                    save_project();
                }

                if (ImGui::MenuItem("Close")) {
                    close_project();
                }
                ImGui::EndMenu();
            }
        },
        0);

    Event::OnDraw::listen([this](Gui&) {
        // TODO: Put this into a keybind register
        if (KeyBindEntry::get(EKeyBind::SAVE).are_all_keys_pressed()) {
            save_project();
        }

        if (KeyBindEntry::get(EKeyBind::CLOSE_PROJECT).are_all_keys_pressed()) {
            close_project();
        }
    });
}

void ProjectManager::mark_as_dirty(const bool dirty) {
    is_dirty = dirty;

    auto &gui = Gui::the();
    if (dirty) {
        gui.set_window_title(fmt::format("structshaper: {}*", project_info.name));
    }
    else {
        gui.set_window_title(fmt::format("structshaper: {}", project_info.name));
    }
}

void ProjectManager::rename_struct(const std::string &old_name, const std::string &new_name) {
    if (!structs.contains(old_name)) {
        throw std::runtime_error("Struct does not exist");
    }

    if (structs.contains(new_name)) {
        throw std::runtime_error("Struct with new name already exists");
    }

    auto node = structs.extract(old_name);
    node.mapped().rename_internal(new_name);
    node.key() = new_name;
    structs.insert(std::move(node));

    mark_as_dirty();
}

void ProjectManager::create_struct(const std::string &name) {
    Struct new_struct(name);

    const auto view = std::make_shared<StructView>();
    view->title = name;

    new_struct.ui_view_id = BaseView::register_view(view);
    structs.emplace(new_struct.name, new_struct);

    LOG_INFO("Added struct {}: ui_idx: {}", new_struct.name, new_struct.ui_view_id);

    mark_as_dirty();
}

void ProjectManager::delete_struct(const std::string &name) {
    if (!structs.contains(name)) {
        throw std::runtime_error("Struct does not exist");
    }

    structs[name].deregister_struct_view();
    structs.erase(name);

    mark_as_dirty();
}

void ProjectManager::create_project(const ProjectInfo &new_project_info) {
    if (new_project_info.name.empty()) {
        throw std::runtime_error("Project name can't be empty!");
    }

    if (new_project_info.process_interface_path.empty()) {
        throw std::runtime_error("Project memory interface can't be empty!");
    }

    project_info = new_project_info;
    Event::ProjectLoaded::call(project_info);

    mark_as_dirty();
}

void ProjectManager::load_project(const std::string_view name) {
    std::ifstream file(project_path(name));
    if (!file.is_open()) {
        throw formatted_error("Project {} does not exist", name);
    }

    auto json = nlohmann::json::parse(file);
    project_info.deserialize(json.at("project_info"));
    project_info.name = name;

    auto &structs_json = json.at("structs");
    for (auto &[struct_name, struct_json] : structs_json.items()) {
        structs.emplace(struct_name, Struct(struct_name));
    }

    for (auto& [struct_name, entry]: structs) {
        try {
            auto& struct_json = structs_json.at(struct_name);
            entry.deserialize(struct_json);

            const auto view = std::make_shared<StructView>();
            view->title = struct_name;
            view->deserialize(struct_json);

            entry.ui_view_id = BaseView::register_view(view);

            LOG_INFO("Loaded struct {}: ui_view_id: {}", struct_name, entry.ui_view_id);
        }
        catch (const std::exception &e) {
            LOG_ERROR("Failed to load struct: {}", e.what());
        }
    }

    Event::ProjectLoaded::call(project_info);
}

void ProjectManager::save_project() {
    if (!is_project_loaded()) {
        return;
    }

    nlohmann::json json{};
    project_info.serialize(json["project_info"]);

    auto &structs_json = json["structs"];
    for (auto &[name, entry] : structs) {
        nlohmann::json struct_json{};
        entry.serialize(struct_json);
        structs_json[name] = struct_json;
    }

    std::ofstream file(project_path(project_info.name));
    file << json.dump(4);
    mark_as_dirty(false);

    LOG_INFO("Saved project: {}", project_info.name);
    Gui::the().add_notification(project_info.name, "Project saved successfully!");
}

void ProjectManager::close_project(const bool close_after_save) {
    const auto close_project = [&] {
        for (const auto &struct_data : structs | std::views::values) {
            struct_data.deregister_struct_view();
        }

        project_info = {};

        structs.clear();
        is_dirty = false;

        Gui::the().set_window_title("structshaper");
        BaseView::close_views(EViewType::MAIN_VIEW);
        BaseView::close_views(EViewType::BACKGROUND_VIEW);

        BaseView::show_view(view_ProjectView);
    };

    if (!is_dirty) {
        close_project();
        return;
    }

    MessageBoxData msg_box{};
    msg_box.title = "Close project " + project_info.name + "?";
    msg_box.message = fmt::format("Do you want to save the project '{}'?", project_info.name);

    msg_box.yes_text = "Yes";
    msg_box.no_text = "No";

    msg_box.ok_callback = [this, close_project, close_after_save] {
        save_project();
        close_project();

        if (close_after_save) {
            glfwSetWindowShouldClose(Gui::the().glfw_window, GLFW_TRUE);
        }
    };

    msg_box.cancel_callback = [this, close_project, close_after_save] {
        close_project();

        if (close_after_save) {
            glfwSetWindowShouldClose(Gui::the().glfw_window, GLFW_TRUE);
        }
    };

    PopupView::show_popup(popup_MessageBoxPopup, std::make_shared<MessageBoxData>(msg_box));
}
