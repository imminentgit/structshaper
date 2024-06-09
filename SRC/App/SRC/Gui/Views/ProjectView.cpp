#include "ProjectView.h"

#include <filesystem>
#include <fstream>

#include <Gui/Gui.h>
#include <Gui/IconsLoader/IconsLoader.h>

#include "Popups/MessageBoxPopup.h"
#include "Popups/CreateProjectPopup.h"

#include <Logger/Logger.h>

#include <Utils/ScopeGuard.h>

ProjectView::ProjectView() : BaseBackgroundView("Projects", EViewType::BACKGROUND_VIEW) {
    using namespace ImGui::Helpers;
}

bool ProjectView::begin() {
    using namespace ImGui::Helpers;

    {
        const auto win_size = Gui::the().main_window_size_with_bar_h();
        ScopedCursorPos scp({0.f, win_size.y * 0.15f - 16.f});
        aligned(CENTER_HORIZONTAL, win_size, [] {
            ImGui::BeginGroup();
            const auto scaled_size = 32.f * Gui::the().scale_factor;
            draw_image("class_info_color", {scaled_size, scaled_size});
            ImGui::SameLine();
            ImGui::PushFont(Gui::the().large_font);
            ImGui::TextUnformatted("Welcome to structshaper!");
            ImGui::PopFont();
            ImGui::EndGroup();
        });
    }

    return BaseBackgroundView::begin();
}

void ProjectView::update_project_list() {
    m_projects.clear();

    for (const auto &project_file : std::filesystem::directory_iterator(ProjectManager::PROJECTS_PATH)) {
        if (!project_file.is_regular_file() || project_file.path().extension() != ".sproj") {
            continue;
        }

        try {
            const auto &project_path = project_file.path();

            std::ifstream file(project_path);
            auto json = nlohmann::json::parse(file);

            ProjectInfo project_info;
            project_info.deserialize(json["project_info"]);
            project_info.path = project_path.string();
            project_info.name = project_path.stem().string();

            const auto now = std::chrono::system_clock::now();
            const auto file_system_time =
                std::chrono::clock_cast<std::chrono::system_clock>(last_write_time(project_path));

            project_info.last_modified = now - file_system_time;

            m_projects.emplace_back(project_info);
        }
        catch (const std::exception &e) {
            LOG_WARN("Failed to load project info from file: {}", e.what());
        }
    }

    std::ranges::sort(
        m_projects, [](const ProjectInfo &lhs, const ProjectInfo &rhs) { return lhs.last_modified < rhs.last_modified; });
}

void ProjectView::render() {
    using namespace ImGui::Helpers;

    BaseView::render();

    if (std::filesystem::is_empty(ProjectManager::PROJECTS_PATH)) {
        aligned_win_size(CENTER_BOTH, [] { ImGui::TextUnformatted("No projects found."); });

        if (aligned_win_size(CENTER_HORIZONTAL, [] { return link_text("Do you want to create a new project?"); })) {
            PopupView::show_popup(popup_CreateProjectPopup);
        }
        return;
    }

    if (m_projects.empty()) {
        update_project_list();
    }

    const auto inner_space = ImGui::GetStyle().FramePadding;
    for (auto &project : m_projects) {
        auto cursor_pos = ImGui::GetCursorScreenPos();
        ImGui::SetNextWindowPos(cursor_pos + inner_space, ImGuiCond_Always);

        ImGui::BeginChild(project.name, {-inner_space.x * 2.f, 0.f},
                          ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_MenuBar);

        ScopeGuard child_guard(&ImGui::EndChild);

        if (ImGui::BeginMenuBar()) {
            Button delete_button = {
                .id = make_imgui_id("_DeleteProjectButton_" + project.name),
                .image = "delete",
                .tooltip = "Deletes the project",
                .size = Gui::menubar_wh(),
                .image_size = DEFAULT_UI_MENUBAR_BUTTON_SIZE,
                .offset = {1.f, 2.f},
                .on_click = [this, &project] {
                    MessageBoxData msg_box{};
                    msg_box.title = "Delete project";

                    msg_box.yes_text = "Yes";
                    msg_box.no_text = "No";

                    msg_box.message = fmt::format("Are you sure you want to delete the project '{}'?", project.name);

                    msg_box.ok_callback = [this, &project] {
                        std::filesystem::remove(project.path);
                        update_project_list();
                    };

                    PopupView::show_popup(popup_MessageBoxPopup, std::make_shared<MessageBoxData>(msg_box));
                },
            };

            draw_buttons_list({delete_button}, true);

            ImGui::TextUnformatted(project.name);
            ImGui::EndMenuBar();
        }

        if (!project.description.empty()) {
            ImGui::TextUnformatted(project.description);
        }

        const auto days = std::chrono::duration_cast<std::chrono::days>(project.last_modified).count();
        const auto time_str = days > 0 ? fmt::format("Last modified: {} days ago", days) : "Last modified: today";
        ImGui::TextUnformatted(time_str);
        ImGui::SameLine();

        if (aligned_win_size(RIGHT, [] { return ImGui::Button("Open"); })) {
            try {
                ProjectManager::the().load_project(project.name);
            }
            catch (const std::exception &e) {
                Gui::the().add_notification("Failed to load project", e.what());
            }
        }
    }
}

void ProjectView::on_show() { update_project_list(); }

void ProjectView::on_buttons_add() {
    using namespace ImGui::Helpers;

    BaseView::on_buttons_add();

    const auto buttons_size = Gui::menubar_wh();
    Button create_project_button = {.id = "##CreateProjectButton",
                                    .image = "add_file",
                                    .tooltip = "Create a new project",
                                    .size = buttons_size,
                                    .image_size = DEFAULT_UI_MENUBAR_BUTTON_SIZE,
                                    .offset = {1.f, 2.f},
                                    .on_click = [] { PopupView::show_popup(popup_CreateProjectPopup); }};

    Button refresh_project_list_button = {.id = "##RefreshProjectListButton",
                                          .image = "refresh",
                                          .tooltip = "Refresh the project list",
                                          .size = buttons_size,
                                          .image_size = DEFAULT_UI_MENUBAR_BUTTON_SIZE,
                                          .offset = {2.f, 2.f},
                                          .on_click = [this] { update_project_list(); }};

    buttons.emplace_back(create_project_button);
    buttons.emplace_back(refresh_project_list_button);
}
