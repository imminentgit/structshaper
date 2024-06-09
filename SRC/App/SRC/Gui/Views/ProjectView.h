#pragma once
#include "BaseView.h"

#include <memory>

#include <ProjectManager/ProjectManager.h>

class ProjectView : public BaseBackgroundView {
    std::vector<ProjectInfo> m_projects{};

    void update_project_list();
public:
    ProjectView();

    bool begin() override;

    void render() override;

    void on_show() override;

    void on_buttons_add() override;
};

REGISTER_VIEW(ProjectView);
