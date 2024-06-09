#pragma once
#include "PopupView.h"

struct SettingsPopup : PopupView {
    SettingsPopup();

    void render() override;

    bool on_close(bool after_fade) override;
};

REGISTER_POPUP(SettingsPopup);
