#pragma once

#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include <imgui.h>

#include <misc/freetype/imgui_freetype.h>

#include <Defines.h>

#include <Gui/Views/BaseView.h>

#include <Gui/Views/Popups/PopupView.h>

#include <Utils/IDAllocator.h>

#include <Utils/StaticInstance.h>

static ImFontConfig &default_font_config() {
    static ImFontConfig cfg{};
    cfg.OversampleH = cfg.OversampleV = 3;
    cfg.RasterizerMultiply = 1.25f;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LoadColor;

    return cfg;
}

inline constexpr auto GUI_ROUNDING_RADIUS = 4.f;
inline constexpr auto DEFAULT_UI_FONT_SIZE =
#ifdef STRUCTSHAPER_IS_WINDOWS
    14.f;
#else
        16.f;
#endif

inline constexpr ImVec2 DEFAULT_UI_MENUBAR_BUTTON_SIZE =
#ifdef STRUCTSHAPER_IS_WINDOWS
    {16.f, 16.f};
#else
    {18.f, 18.f};
#endif
inline constexpr auto GUI_MAIN_ID = "StructShaper_MainWindow";

struct EFontFlags {
    uint8_t is_monospace : 1;
    uint8_t is_italic : 1;
};

struct FontEntry {
    uint32_t recommended_weight{};
    std::string type_face_name{};
    std::string file_path{};
    EFontFlags flags{};

    ImFont* preview_font{};
};

enum class EFadeState { FADE_IN, FADE_OUT };

struct NotificationEntry {
    std::string title{};

    std::string message{};

    std::chrono::system_clock::time_point creation_time{};
};

struct Gui : StaticInstance<Gui> {
    GLFWwindow *glfw_window{};
    ImGuiContext *imgui_context{};

    std::string window_title{"structshaper"};

    bool run_as_borderless{};
    float scale_factor = 1.f;

    bool is_resizing_manually{};
    bool is_in_popup{};

    ImVec2 main_window_size{800.f, 550.f};

    ImGuiID dockspace_id{};
    ImGuiID dockspace_left_id, dockspace_right_id{}, dockspace_bottom_id{};

    std::vector<GLuint> textures{};

    std::map<std::string, FontEntry> installed_fonts{};
    FontEntry* default_font_entry, *default_monospace_font_entry{};
    ImFont *default_font, *large_font{}, * monospace_font{};

    UniversalId fonts_built_flag{};

    IdAllocator<100> view_id_allocator{};
    std::unordered_map<UniversalId, BaseViewPtr> views{};

    std::vector<PopupViewPtr> popups{};
    std::stack<std::pair<size_t, std::shared_ptr<void>>> popup_stack{};
    size_t queued_popups{};

    bool do_fade{};
    EFadeState fade_state = EFadeState::FADE_IN;
    int fade_in_alpha{};

    std::vector<NotificationEntry> notifications{};

    [[nodiscard]] ALWAYS_INLINE ImVec2 main_window_size_with_bar_h() const {
        return {main_window_size.x, main_window_size.y + ImGui::GetFrameHeight()};
    }

    [[nodiscard]] static ImVec2 menubar_wh() { return {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()}; }
private:
    void init_imgui();

    void setup_glfw_hints() const;

    void render_menu_bar_items() const;

    void render_menu_bar();

    void build_fonts_preview();

    void pre_render();

    void render_notifications();

    void render();

    void post_render();

    ALWAYS_INLINE void full_draw() {
        pre_render();
        render();
        post_render();
    }

    void loop();

    void end();

    void render_views(EViewType filter) const;

    void pop_popups();

    void render_popups();

public:
    void init();

    GLuint create_texture(uint32_t width, uint32_t height);

    static void update_texture_rgba(GLuint texture, uint32_t width, uint32_t height, const void *data);

    void set_window_title(std::string_view title);

    void add_notification(std::string_view title, std::string_view message);
private:
    void native_grab_fonts();

    void native_init() const;

    void native_end() const;

    void native_pre_render() const;

    void native_post_render();

#ifdef STRUCTSHAPER_IS_WINDOWS
    void push_acrylic_color() const;

    void pop_acrylic_color() const;
#else
    bool grab_fonts_via_fc();
#endif
};
