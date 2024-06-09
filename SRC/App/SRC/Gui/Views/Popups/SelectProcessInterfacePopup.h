#pragma once
#include "PopupView.h"

#include <ProjectManager/ProjectManager.h>

struct SelectProcessInterfacePopup : PopupView {
    SelectProcessInterfacePopup();

    void render() override;
};

REGISTER_POPUP(SelectProcessInterfacePopup);
