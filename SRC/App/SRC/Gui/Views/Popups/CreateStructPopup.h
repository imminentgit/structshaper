#pragma once
#include "PopupView.h"

class CreateStructPopup : public PopupView {
    std::string m_name{};
public:
    CreateStructPopup();

    void render() override;
};

REGISTER_POPUP(CreateStructPopup);
