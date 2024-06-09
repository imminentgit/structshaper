#include "Gui.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <imgui_internal.h>

#include <Gui/IconsLoader/IconsLoader.h>

#include <Gui/Views/ProjectView.h>

#include <OptionsManager/OptionsManager.h>

#include <Logger/Logger.h>
#include <Queue/Event.h>
#include <Utils/ScopeGuard.h>

void Gui::setup_glfw_hints() const {
    // We need at least OpenGL 3.0
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

    // We want the OpenGL context to be forward compatible
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Transparent background?
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, run_as_borderless ? GLFW_TRUE : GLFW_FALSE);

    // Set borderless window hint
    glfwWindowHint(GLFW_DECORATED, run_as_borderless ? GLFW_FALSE : GLFW_TRUE);

    // Set the window to be invisible first
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
}

void Gui::init() {
    glfwSetErrorCallback(
        [](int error, const char *description) { LOG_ERROR("GLFW error: {:#X} -> {}", error, description); });

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    setup_glfw_hints();

    glfw_window = glfwCreateWindow(static_cast<int>(main_window_size.x), static_cast<int>(main_window_size.y),
                                   window_title.c_str(), nullptr, nullptr);
    if (!glfw_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

#ifdef STRUCTSHAPER_IS_WINDOWS
    glfwSetWindowPos(glfw_window, 150, 150);
#endif

    glfwSetWindowSizeLimits(glfw_window, static_cast<int>(300 * scale_factor), static_cast<int>(200 * scale_factor),
                            GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwSetWindowUserPointer(glfw_window, this);
    glfwMakeContextCurrent(glfw_window);

    glfwSwapInterval(1);
    glfwShowWindow(glfw_window);

    init_imgui();
    native_init();

    LOG_INFO("OpenGL Version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

    loop();
}

void Gui::init_imgui() {
    IMGUI_CHECKVERSION();

    imgui_context = ImGui::CreateContext();
    if (!imgui_context) {
        throw std::runtime_error("Failed to create ImGui context");
    }

    auto &style = ImGui::GetStyle();
    style.DisplaySafeAreaPadding = {};

    auto &io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    imgui_context->DebugLogFlags |= ImGuiDebugLogFlags_EventDocking;

    if (!ImGui_ImplGlfw_InitForOpenGL(glfw_window, true)) {
        throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }

    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        throw std::runtime_error("Failed to initialize ImGui OpenGL3 backend");
    }

    static GLFWwindowposfun s_OldWindowPosCallback =
        glfwSetWindowPosCallback(glfw_window, [](GLFWwindow *window, const int x, const int y) {
            auto &win = *static_cast<Gui *>(glfwGetWindowUserPointer(window));

#ifdef STRUCTSHAPER_IS_WINDOWS
            if (!win.is_resizing_manually) {
                win.full_draw();
            }
#endif
            if (s_OldWindowPosCallback) {
                s_OldWindowPosCallback(window, x, y);
            }
        });

    static GLFWwindowsizefun s_OldWindowSetSizeCallback =
        glfwSetWindowSizeCallback(glfw_window, [](GLFWwindow *window, const int w, const int h) {
            auto &win = *static_cast<Gui *>(glfwGetWindowUserPointer(window));

            if (!glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
                win.main_window_size.x = static_cast<float>(w);
                win.main_window_size.y = static_cast<float>(h);
            }

#ifdef STRUCTSHAPER_IS_WINDOWS
            if (!win.is_resizing_manually) {
                win.full_draw();
            }
#endif
            if (s_OldWindowSetSizeCallback) {
                s_OldWindowSetSizeCallback(window, w, h);
            }
        });

    IconsLoader::init();

    const auto *class_info_color = IconsLoader::get_icon("class_info_color");
    if (class_info_color) {
        auto icon = class_info_color->renderToBitmap(48, 48);
        icon.convertToRGBA();

        GLFWimage image;
        image.width = static_cast<int>(icon.width());
        image.height = static_cast<int>(icon.height());
        image.pixels = icon.data();

        glfwSetWindowIcon(glfw_window, 1, &image);
    }

    const auto &imgui_fonts = io.Fonts;
    native_grab_fonts();

    // Issue: Preview is disabled due to imgui not supporting multiple atlases
    // build_fonts_preview();

    auto &options = OptionsManager::the().options;

#ifdef STRUCTSHAPER_IS_WINDOWS
    default_monospace_font_entry = &installed_fonts.at("C:\\Windows\\Fonts\\consola.ttf");

    default_font_entry = &installed_fonts.at("C:\\Windows\\Fonts\\arial.ttf");
#endif
    options.selected_monospace_font_path = default_monospace_font_entry->file_path;

    monospace_font = imgui_fonts->AddFontFromFileTTF(
        default_monospace_font_entry->file_path,
        static_cast<float>(options.selected_monospace_font_size) * scale_factor, &default_font_config());

    default_font = imgui_fonts->AddFontFromFileTTF(default_font_entry->file_path, DEFAULT_UI_FONT_SIZE * scale_factor,
                                                   &default_font_config());
    large_font =
        imgui_fonts->AddFontFromFileTTF(default_font_entry->file_path, 32.f * scale_factor, &default_font_config());
}

void Gui::build_fonts_preview() {
    for (auto &[file, entry] : installed_fonts) {
        if (!entry.flags.is_monospace) {
            continue;
        }

        entry.preview_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            file, entry.recommended_weight > 500 ? 16.f * scale_factor : 14.f * scale_factor, &default_font_config());
    }
}

void Gui::pre_render() {
    Event::UpdateFont::update_pull_single([this](const std::string &font_path, const int size) {
        const auto &selected_font_entry = installed_fonts.at(font_path);

        monospace_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            selected_font_entry.file_path, static_cast<float>(size) * scale_factor, &default_font_config());

        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        return true;
    });

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    native_pre_render();

    if (default_font) {
        ImGui::PushFont(default_font);
    }

    Event::OnPreDraw::call(*this);
}

void Gui::render_menu_bar_items() const {
    constexpr auto POPUP_ID = "##ProjectMenuPopup";

    const auto cur_pos = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(cur_pos - 4.f);

    if (run_as_borderless && ImGui::Helpers::three_dot_button("##ProjectMenuButton", menubar_wh())) {
        ImGui::OpenPopup(POPUP_ID);
    }

    if (!run_as_borderless || ImGui::BeginPopup(POPUP_ID)) {
        Event::OnMenuBarRender::call();

        if (run_as_borderless) {
            ImGui::EndPopup();
        }
    }
}

void Gui::render_menu_bar() {
    using namespace ImGui::Helpers;

    if (!ImGui::BeginMenuBar()) {
        return;
    }
    ScopeGuard menu_bar_guard(&ImGui::EndMenuBar);
    const auto orig_cursor_pos = ImGui::GetCursorPos();

    if (run_as_borderless) {
        const auto scaled_size = 16.f * scale_factor;

        ImGui::SetCursorPosY(3.f);
        draw_image("class_info_color", {scaled_size, scaled_size});

        ImGui::SetCursorPosY(0.f);
        ImGui::TextUnformatted(window_title);
        ImGui::SetCursorPosY(orig_cursor_pos.y);
    }

    render_menu_bar_items();

    ImGui::SetCursorPos(orig_cursor_pos);
    if (run_as_borderless) {
        auto close_button_size = menubar_wh() / scale_factor;
        const auto scaling_disabled = scale_factor == 1.f;
        if (scaling_disabled) {
            close_button_size += ImVec2(2.f, 2.f);
        }

        Button close_button = {
            .id = "##CloseButton",
            .image = "close",
            .size = menubar_wh(),
            .image_size = close_button_size,
            .offset = {-1.f, scaling_disabled ? 0.f : 1.f},
            .on_click = [this] { glfwSetWindowShouldClose(glfw_window, GLFW_TRUE); },
        };

        Button maximize_button = {
            .id = "##MaximizeButton",
            .image = "maximize",
            .size = menubar_wh(),
            .image_size = menubar_wh() / scale_factor,
            .offset = {1.f, 1.f},
            .on_click =
                [this] {
                    is_resizing_manually = true;
                    if (glfwGetWindowAttrib(glfw_window, GLFW_MAXIMIZED)) {
                        glfwRestoreWindow(glfw_window);
                    }
                    else {
                        glfwMaximizeWindow(glfw_window);
                    }
                },
        };

        Button minimize_button = {
            .id = "##MinimizeButton",
            .image = "minimize",
            .size = menubar_wh(),
            .image_size = menubar_wh() / scale_factor,
            .offset = {2.f, -1.f},
            .on_click =
                [this] {
                    is_resizing_manually = true;
                    glfwIconifyWindow(glfw_window);
                },
        };

        static bool is_pinned = false;
        Button pin_button = {
            .id = "##PinButton",
            .image = "pin",
            .size = menubar_wh(),
            .image_size = menubar_wh() / scale_factor,
            .offset = {0.f, 1.f},
            .highlight = is_pinned,
            .on_click = [this] {
                is_pinned = !is_pinned;

                glfwSetWindowAttrib(glfw_window, GLFW_FLOATING, is_pinned ? GLFW_TRUE : GLFW_FALSE);
            },
        };

        draw_buttons_list({close_button, maximize_button, minimize_button, pin_button}, true);
    }
}

#ifdef STRUCTSHAPER_IS_WINDOWS
void Gui::push_acrylic_color() const {
    if (run_as_borderless) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(16, 16, 16, 164));
    }
}

void Gui::pop_acrylic_color() const {
    if (run_as_borderless) {
        ImGui::PopStyleColor();
    }
}
#endif

void push_style_and_rounding() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, GUI_ROUNDING_RADIUS);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, GUI_ROUNDING_RADIUS);

    const auto col = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, col);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, col);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, col);
}

void pop_style_and_rounding() {
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void Gui::render_views(const EViewType filter) const {
    const auto min_window_size = ImGui::GetStyle().WindowMinSize;

    for (const auto &view : views | std::views::values) {
        if (view->view_type != filter) {
            continue;
        }

        if (!view->should_render) {
            continue;
        }

        const auto open = view->begin();
        const auto size = ImGui::GetWindowSize();
        if (!open || size.x <= min_window_size.x || size.y <= min_window_size.y) {
            view->end();
            continue;
        }

        view->render();
        view->end();
    }
}

void Gui::pop_popups() {
    if (popup_stack.empty()) {
        return;
    }

    if (is_in_popup) {
        return;
    }

    auto [popup_idx, user_data] = popup_stack.top();
    popup_stack.pop();

    auto &popup = popups.at(popup_idx);
    popup->is_open = true;
    popup->user_data = user_data;

    fade_state = EFadeState::FADE_IN;
    do_fade = true;
}

void Gui::render_popups() {
    const auto *viewport = ImGui::GetMainViewport();
    const auto menubar_vec = ImVec2(0, ImGui::GetFrameHeight());

    pop_popups();

    if (do_fade) {
        const auto fade = 360.f * ImGui::GetIO().DeltaTime;
        if (fade_state == EFadeState::FADE_IN) {
            fade_in_alpha += static_cast<int>(fade) + 1;
            if (fade_in_alpha >= 64) {
                fade_in_alpha = 64;
                fade_state = EFadeState::FADE_OUT;
                do_fade = false;
            }
        }
        else {
            fade_in_alpha -= static_cast<int>(fade) + 1;
            if (fade_in_alpha <= 0) {
                fade_in_alpha = 0;
                fade_state = EFadeState::FADE_IN;
                do_fade = false;
            }
        }
    }

    if (fade_in_alpha > 0) {
        ImGui::SetNextWindowPos(viewport->WorkPos + menubar_vec);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin("##InvisibleOverlayWindow", nullptr,
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavFocus |
                             ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos(), ImGui::GetWindowSize(),
                                                      IM_COL32(255, 255, 255, fade_in_alpha));

            ImGui::End();
        }

        ImGui::PopStyleVar();
    }

    for (const auto &popup : popups) {
        if (!popup->is_open) {
            continue;
        }

        const auto col = ImGui::GetColorU32(ImGuiCol_WindowBg, static_cast<float>(fade_in_alpha + 196) / 255.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
        if (popup->begin()) {
            ImGui::PopStyleColor();
            popup->render();

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                popup->close_popup();
            }

            if (popup->does_want_to_close) {
                const auto one_popup = queued_popups == 1;

                if (one_popup) {
                    do_fade = true;
                    fade_state = EFadeState::FADE_OUT;
                }

                if (!one_popup || fade_in_alpha <= 0) {
                    popup->is_open = false;
                    popup->does_want_to_close = false;
                    popup->on_close(true);
                    --queued_popups;
                }
            }

            popup->end();
        }

        is_in_popup = popup->is_open;
    }
}

void Gui::render_notifications() {
    using namespace ImGui::Helpers;

    constexpr auto NOTIFICATION_WINDOW_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoSavedSettings;

    const auto display_size = Gui::the().main_window_size;

    constexpr auto PADDING = 5.f;
    auto offset = 0.f;

    for (size_t i = 0; i < notifications.size(); ++i) {
        const auto &notification = notifications[i];

        const auto time_since_creation = std::chrono::duration_cast<std::chrono::seconds>(
                                             std::chrono::system_clock::now() - notification.creation_time)
                                             .count();

        if (time_since_creation > 3) {
            notifications.erase(notifications.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }

        ImGui::SetNextWindowPos(ImVec2(display_size.x - PADDING, display_size.y - PADDING - offset), ImGuiCond_Always,
                                ImVec2(1.0f, 1.0f));

        const auto id = fmt::format("##Notification_{}_{}", notification.title, i);
        if (ImGui::Begin(id, nullptr, NOTIFICATION_WINDOW_FLAGS)) {
            if (!notification.title.empty() && notification.title[0] != '#') {
                ImGui::PushTextWrapPos(display_size.x * 0.75f);
                ImGui::TextUnformatted(notification.title);
            }

            if (!notification.message.empty()) {
                ImGui::PushTextWrapPos(display_size.x * 0.75f);
                ImGui::TextUnformatted(notification.message);
            }

            offset += ImGui::GetWindowHeight() + PADDING;

            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            ImGui::End();
        }
    }
}

void Gui::render() {
    auto push_paddings = [this] {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
#ifdef STRUCTSHAPER_IS_WINDOWS
                            0.f);
#elif STRUCTSHAPER_IS_LINUX
                            run_as_borderless ? GUI_ROUNDING_RADIUS : 0.0f);
#endif

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,
#ifdef STRUCTSHAPER_IS_WINDOWS
                            0.0f);
#elif STRUCTSHAPER_IS_LINUX
                            run_as_borderless ? 1.0f : 0.0f);
#endif
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    };

    auto pop_paddings = [] { ImGui::PopStyleVar(3); };

    constexpr auto WINDOW_FLAGS = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.f);

    push_paddings();
#ifdef STRUCTSHAPER_IS_WINDOWS
    push_acrylic_color();
#endif
    ImGui::Begin(GUI_MAIN_ID, nullptr, WINDOW_FLAGS);
    {
#ifdef STRUCTSHAPER_IS_WINDOWS
        pop_acrylic_color();
#endif

        pop_paddings();

        push_style_and_rounding();
        { render_views(EViewType::BACKGROUND_VIEW); }
        pop_style_and_rounding();

        {
            constexpr auto DOCK_FLAGS = ImGuiDockNodeFlags_PassthruCentralNode;
            dockspace_id = ImGui::GetID("MainDockSpace");

            const auto menubar_vec = ImVec2(0, ImGui::GetFrameHeight());
            const auto dockspace_size = viewport->WorkSize - menubar_vec;
            const auto dock_start = viewport->Pos + menubar_vec;

            static bool does_node_exist = false; // ImGui::DockBuilderGetNode(dockspace_id);

            ImGui::Helpers::ScopedCursorPos scp(dock_start);
            ImGui::DockSpace(dockspace_id, dockspace_size, DOCK_FLAGS);

            if (!does_node_exist) {
                does_node_exist = true;
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id,
                                          DOCK_FLAGS | static_cast<ImGuiDockNodeFlags_>(ImGuiDockNodeFlags_DockSpace));
                ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

                ImGuiID dock_space_inside = dockspace_id;
                dockspace_left_id =
                    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, &dock_space_inside, &dockspace_id);

                auto *left_node = ImGui::DockBuilderGetNode(dockspace_left_id);
                left_node->SetLocalFlags(left_node->LocalFlags | ImGuiDockNodeFlags_HiddenTabBar);

                dockspace_right_id =
                    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.8f, nullptr, &dockspace_id);

                dockspace_bottom_id =
                    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.2f, nullptr, &dockspace_id);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        render_menu_bar();

        ImGui::End();
    }

    push_style_and_rounding();
#ifdef STRUCTSHAPER_IS_WINDOWS
    push_acrylic_color();
#endif
    {
        render_views(EViewType::MAIN_VIEW);
        render_popups();
    }
#ifdef STRUCTSHAPER_IS_WINDOWS
    pop_acrylic_color();
#endif
    pop_style_and_rounding();

    ImGui::PopStyleVar();

    Event::OnDraw::call(*this);

#ifdef STRUCTSHAPER_DEV
    // ImGui::ShowDebugLogWindow();
//
// ImGui::ShowDemoWindow(nullptr);
#endif

    render_notifications();
}

void imgui_error_callback(void *user_data, const char *fmt, ...) {
    auto size = snprintf(nullptr, 0, fmt, nullptr);
    std::string buf(size, '\0');
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf.data(), size + 1, fmt, args);
    va_end(args);

    LOG_INFO("ImGui error: {}", buf);
}

void Gui::post_render() {
    Event::OnPostDraw::call(*this);

    native_post_render();

    if (default_font) {
        ImGui::PopFont();
    }

    ImGui::ErrorCheckEndFrameRecover(imgui_error_callback);

    ImGui::Render();

    auto *backup = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup);

    int w, h;
    glfwGetFramebufferSize(glfw_window, &w, &h);

    glViewport(0, 0, w, h);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto *draw_data = ImGui::GetDrawData();
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    glfwSwapBuffers(glfw_window);

    is_resizing_manually = false;
}

void Gui::loop() {
    // TODO: Optimize further
    while (!glfwWindowShouldClose(glfw_window)) {
        if (!glfwGetWindowAttrib(glfw_window, GLFW_VISIBLE) || glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED))
            [[unlikely]] {
            glfwWaitEvents();
            continue;
        }

        full_draw();

        // glfwPollEvents();
        glfwPollEvents();
    }

    end();
}

void Gui::end() {
    native_end();

    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfw_window);
    glfwTerminate();

    glfw_window = nullptr;
}

GLuint Gui::create_texture(const uint32_t width, const uint32_t height) {
    GLuint texture{};

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    std::vector<uint8_t> rgba_buf(width * height * 4, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLint>(width), static_cast<GLint>(height), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);

    textures.push_back(texture);
    return texture;
}

void Gui::update_texture_rgba(const GLuint texture, const uint32_t width, const uint32_t height, const void *data) {
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLint>(width), static_cast<GLint>(height), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Gui::set_window_title(const std::string_view title) {
    window_title = title;
    glfwSetWindowTitle(glfw_window, window_title.data());
}

void Gui::add_notification(const std::string_view title, const std::string_view message) {
    NotificationEntry entry{};
    entry.title = title;
    entry.message = message;
    entry.creation_time = std::chrono::system_clock::now();
    notifications.emplace_back(entry);
}
