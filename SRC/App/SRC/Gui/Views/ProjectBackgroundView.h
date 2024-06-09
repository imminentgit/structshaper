#pragma once
#include "BaseView.h"

#include <memory>

struct ProjectBackgroundView : BaseBackgroundView {
    ProjectBackgroundView();

    void render() override;
};

REGISTER_VIEW(ProjectBackgroundView);
