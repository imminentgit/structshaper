#pragma once

#include <memory>
#include <string>

#include <imgui.h>

#include <fmt/format.h>

#include <Gui/ImGuiHelpers/ImGuiHelpers.h>

#include <Utils/IDAllocator.h>

enum class EViewType { BACKGROUND_VIEW, MAIN_VIEW, POPUP_VIEW };

struct BaseView;
using BaseViewPtr = std::shared_ptr<BaseView>;

struct EViewSizeBehavior {
    uint8_t AUTO_SIZE_X : 1;
    uint8_t AUTO_SIZE_Y : 1;
    uint8_t HALF_MAIN_WINDOW_X : 1;
    uint8_t HALF_MAIN_WINDOW_Y : 1;
    uint8_t CUSTOM_X : 1;
    uint8_t CUSTOM_Y : 1;
};

struct BaseView {
    std::string title{};

    UniversalId id{};

    ImVec2 size{};

    EViewSizeBehavior size_behavior = {.AUTO_SIZE_X = true, .AUTO_SIZE_Y = true};

    EViewType view_type{};

    ImGui::Helpers::ButtonsVecType buttons{};

    bool should_render = true;
    bool has_added_buttons = false;

    bool is_docked = false;
    bool did_push = false;

    static UniversalId register_view(BaseViewPtr &&view);

    static void remove_view(UniversalId id);

    static void close_views(EViewType type);

    static void show_view(UniversalId id, bool show = true);

    explicit BaseView(std::string title, const EViewType type) : title(std::move(title)), view_type(type) {}

    virtual ~BaseView() = default;

    virtual void on_show() {}

    virtual void on_buttons_add() {}

    virtual bool begin();

    virtual void render();

    virtual void end();

    virtual bool begin_docked_callback_inner() { return false; }

    virtual void end_docked_callback_inner() {}

    // WARNING: Make sure to call this in the derived class if you override begin() !
    // TODO: Maybe add a before begin bool?
    void begin_docked_callback() { did_push = begin_docked_callback_inner(); }

    // WARNING: Make sure to call this in the derived class if you override end() !
    void end_docked_callback() {
        if (did_push) {
            end_docked_callback_inner();
            did_push = false;
        }
    }

    template <typename ...TArgs>
    std::string make_imgui_id(const std::string_view other, TArgs &&...args) {
        return fmt::format("##{}_{}_{}", id, title, fmt::vformat(other, fmt::make_format_args(args...)));
    }
};

struct BaseBackgroundView : BaseView {
    bool invisible_covering_window = false;

    explicit BaseBackgroundView(std::string title, const EViewType type) : BaseView(std::move(title), type) {}

    bool begin() override;

    void end() override;
};

#define REGISTER_VIEW(view_class, ...)                                                                                 \
    [[maybe_unused]] inline size_t view_##view_class =                                                                 \
        BaseView::register_view(std::make_shared<view_class>(__VA_ARGS__));
