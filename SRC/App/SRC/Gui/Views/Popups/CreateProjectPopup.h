#pragma once
#include "PopupView.h"

#include <ProjectManager/ProjectManager.h>

class CreateProjectPopup : public PopupView {
    ProjectInfo m_new_project_info{};
    int m_selected_interface = 0;
public:
    CreateProjectPopup();

    void render() override;

    bool on_close(bool after_fade) override;
};

REGISTER_POPUP(CreateProjectPopup);
