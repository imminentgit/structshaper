#pragma once
#include "../BaseView.h"

struct PopupView;
using PopupViewPtr = std::shared_ptr<PopupView>;

struct PopupView : BaseView {
    bool is_open = false;
    bool does_want_to_close = false;
    bool scrolling = true;
    bool horizontal_scrollbar = false;
    std::shared_ptr<void> user_data{};

    static size_t register_popup(PopupViewPtr &&view);

    static void show_popup(size_t idx, const std::shared_ptr<void>& data = nullptr);

    void close_popup() {
        does_want_to_close = on_close(false);
    }

    PopupView(const std::string &title) : BaseView(title, EViewType::POPUP_VIEW) {}

    bool begin() override;

    void render() override;

    void end() override;

    virtual bool on_close(bool after_fade) { return true; }

    void on_buttons_add() override;
};

#define REGISTER_POPUP(popup_class, ...) \
        [[maybe_unused]] inline size_t popup_##popup_class = PopupView::register_popup(std::make_shared<popup_class>(__VA_ARGS__)); \

