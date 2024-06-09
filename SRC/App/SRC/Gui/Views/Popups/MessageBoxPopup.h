#pragma once
#include "PopupView.h"

#include <functional>

struct MessageBoxData {
    std::string title{};
    std::string message{};

    std::string yes_text = "Confirm";
    std::string no_text = "Cancel";

    std::function<void()> ok_callback{};
    std::function<void()> cancel_callback{};
};

struct MessageBoxPopup : PopupView {
    MessageBoxPopup();

    void render() override;

    bool on_close(bool after_fade) override;
};

REGISTER_POPUP(MessageBoxPopup);
