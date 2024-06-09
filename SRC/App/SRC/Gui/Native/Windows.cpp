#ifdef STRUCTSHAPER_IS_WINDOWS
    #include <dwmapi.h>
    #include <windows.h>
    #include <windowsx.h>

    #include <filesystem>
    #include <map>

    #include "../Gui.h"

    #include <Logger/Logger.h>
    #include <Queue/Event.h>
    #include <Utils/ScopeGuard.h>
    #include <Utils/Utils.h>

    #define GLFW_EXPOSE_NATIVE_WIN32

    #include <GLFW/glfw3native.h>

    #undef GLFW_EXPOSE_NATIVE_WIN32

    #include <imgui_internal.h>

extern "C" BOOL __stdcall GetFontResourceInfoW(LPCWSTR, LPDWORD, LPVOID, DWORD);

// Source: https://gist.github.com/sylveon/9c199bb6684fe7dffcba1e3d383fb609
enum ACCENT_STATE : INT {                  // Affects the rendering of the background of a window.
    ACCENT_DISABLED = 0,                   // Default value. Background is black.
    ACCENT_ENABLE_GRADIENT = 1,            // Background is GradientColor, alpha channel ignored.
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2, // Background is GradientColor.
    ACCENT_ENABLE_BLURBEHIND = 3,          // Background is GradientColor, with blur effect.
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,   // Background is GradientColor, with acrylic blur effect.
    ACCENT_ENABLE_HOSTBACKDROP = 5,        // Unknown.
    ACCENT_INVALID_STATE = 6               // Unknown. Seems to draw background fully transparent.
};

struct ACCENT_POLICY {        // Determines how a window's background is rendered.
    ACCENT_STATE AccentState; // Background effect.
    UINT AccentFlags;         // Flags. Set to 2 to tell GradientColor is used, rest is unknown.
    COLORREF GradientColor;   // Background color.
    LONG AnimationId;         // Unknown
};

enum WINDOWCOMPOSITIONATTRIB : INT { // Determines what attribute is being manipulated.
    WCA_ACCENT_POLICY = 0x13         // The attribute being get or set is an accent policy.
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB dwAttrib;
    PVOID pvData;
    SIZE_T cbData;
};

BOOL WINAPI SetWindowCompositionAttribute(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA *attrib) {
    static auto func = reinterpret_cast<BOOL(WINAPI *)(HWND, WINDOWCOMPOSITIONATTRIBDATA *)>(
        GetProcAddress(LoadLibraryA("user32.dll"), "SetWindowCompositionAttribute"));

    return func ? func(hwnd, attrib) : FALSE;
}

VOID WINAPI RtlGetVersion(LPOSVERSIONINFOW lpVersionInformation) {
    static auto func = reinterpret_cast<VOID(WINAPI *)(LPOSVERSIONINFOW)>(
        GetProcAddress(LoadLibraryA("ntdll.dll"), "RtlGetVersion"));

    if (func) {
        func(lpVersionInformation);
    }
}

WNDPROC s_old_wndproc{};
bool s_ignore_resize_area{};

static LRESULT normal_wnd_proc_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CLOSE: {
        auto& proj_manager = ProjectManager::the();
        if (proj_manager.is_dirty) {
            ProjectManager::the().close_project(true);
            return 0;
        }

        break;
    }
    case WM_SETCURSOR: {
        if (LOWORD(lparam) == HTCLIENT) {
            switch (ImGui::GetMouseCursor()) {
            case ImGuiMouseCursor_Arrow:
                SetCursor(LoadCursorA(nullptr, IDC_ARROW));
                return TRUE;
            case ImGuiMouseCursor_Hand:
                SetCursor(LoadCursorA(nullptr, IDC_HAND));
                return TRUE;
            case ImGuiMouseCursor_ResizeEW:
                SetCursor(LoadCursorA(nullptr, IDC_SIZEWE));
                return TRUE;
            case ImGuiMouseCursor_ResizeNS:
                SetCursor(LoadCursorA(nullptr, IDC_SIZENS));
                return TRUE;
            case ImGuiMouseCursor_ResizeNWSE:
                SetCursor(LoadCursorA(nullptr, IDC_SIZENWSE));
                return TRUE;
            case ImGuiMouseCursor_ResizeNESW:
                SetCursor(LoadCursorA(nullptr, IDC_SIZENESW));
                return TRUE;
            case ImGuiMouseCursor_ResizeAll:
                SetCursor(LoadCursorA(nullptr, IDC_SIZEALL));
                return TRUE;
            case ImGuiMouseCursor_NotAllowed:
                SetCursor(LoadCursorA(nullptr, IDC_NO));
                return TRUE;
            case ImGuiMouseCursor_TextInput:
                SetCursor(LoadCursorA(nullptr, IDC_IBEAM));
                return TRUE;
            default:
                break;
            }
        }
        break;
    }
    default:
        break;
    }

    return CallWindowProcW(s_old_wndproc, hwnd, msg, wparam, lparam);
}

static LRESULT borderless_wnd_proc_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_MOUSELAST:
        break;
    case WM_NCACTIVATE:
    case WM_NCPAINT:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    case WM_NCCALCSIZE: {
        auto &rect = *reinterpret_cast<RECT *>(lparam);
        const auto client = rect;

        CallWindowProcW(s_old_wndproc, hwnd, msg, wparam, lparam);

        if (IsMaximized(hwnd)) {
            WINDOWINFO windowInfo{.cbSize = sizeof(WINDOWINFO)};
            GetWindowInfo(hwnd, &windowInfo);

            rect = {.left = client.left + static_cast<LONG>(windowInfo.cyWindowBorders),
                    .top = client.top + static_cast<LONG>(windowInfo.cyWindowBorders),
                    .right = client.right - static_cast<LONG>(windowInfo.cyWindowBorders),
                    .bottom = client.bottom - static_cast<LONG>(windowInfo.cyWindowBorders + 1)};
        }
        else {
            rect = client;
        }

        return FALSE;
    }
    case WM_NCHITTEST: {
        const POINT cursor = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

        const POINT border{::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
                           ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)};

        auto &gui = Gui::the();
        auto *glfw_window = gui.glfw_window;
        if (glfwGetWindowMonitor(glfw_window)) {
            return HTCLIENT;
        }

        RECT window;
        if (!GetWindowRect(hwnd, &window)) {
            return HTNOWHERE;
        }

        enum ERegionMask {
            CLIENT = 0b0000,
            LEFT = 0b0001,
            RIGHT = 0b0010,
            TOP = 0b0100,
            BOTTOM = 0b1000,
        };

        auto result = LEFT * (cursor.x < window.left + border.x) | RIGHT * (cursor.x >= window.right - border.x) |
            TOP * (cursor.y < window.top + border.y) | BOTTOM * (cursor.y >= window.bottom - border.y);

        if (s_ignore_resize_area) {
            result = CLIENT;
        }

        switch (result) {
        case LEFT:
            return HTLEFT;
        case RIGHT:
            return HTRIGHT;
        case TOP:
            return HTTOP;
        case BOTTOM:
            return HTBOTTOM;
        case TOP | LEFT:
            return HTTOPLEFT;
        case TOP | RIGHT:
            return HTTOPRIGHT;
        case BOTTOM | LEFT:
            return HTBOTTOMLEFT;
        case BOTTOM | RIGHT:
            return HTBOTTOMRIGHT;
        case CLIENT: {
            const auto *hovered_window = Gui::the().imgui_context->HoveredWindow;
            const std::string_view hovered_win = hovered_window != nullptr ? hovered_window->Name : "";
            if (hovered_win == GUI_MAIN_ID && !ImGui::IsAnyItemHovered() &&
                cursor.y < window.top + static_cast<LONG>(ImGui::GetFrameHeight())) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        default:
            return HTNOWHERE;
        }
    }

    default:
        break;
    }

    return normal_wnd_proc_handler(hwnd, msg, wparam, lparam);
}

void Gui::native_pre_render() const {
    if (run_as_borderless) {
        const auto hwnd = glfwGetWin32Window(glfw_window);
        SetWindowLongW(hwnd, GWL_STYLE,
                       (GetWindowLongW(hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW) & ~static_cast<LONG>(WS_POPUP));

        s_ignore_resize_area = ImGui::IsKeyDown(ImGuiKey_LeftAlt);
    }
}

void Gui::native_post_render() {}

void Gui::native_grab_fonts() {
    for (auto &path : std::filesystem::directory_iterator("C:\\Windows\\Fonts")) {
        if (!path.is_regular_file() || path.path().extension() != ".ttf") {
            continue;
        }

        const auto &path_str = path.path().string();
        const auto wide_path_str = Utils::string_to_wstring(path_str);

        if (AddFontResourceExW(wide_path_str.c_str(), FR_PRIVATE, nullptr) == 0) {
            continue;
        }

        ScopeGuard font_guard([path_str] { RemoveFontResourceExA(path_str.c_str(), FR_PRIVATE, nullptr); });

        DWORD size = 0;
        if (GetFontResourceInfoW(wide_path_str.c_str(), &size, nullptr, 2) == 0 || size < sizeof(LOGFONTW)) {
            continue;
        }

        std::vector<uint8_t> buffer(size);
        if (GetFontResourceInfoW(wide_path_str.c_str(), &size, buffer.data(), 2) == 0) {
            continue;
        }

        for (size_t i = 0; i < buffer.size() / sizeof(LOGFONTW); ++i) {
            const auto &font = reinterpret_cast<LOGFONTW *>(buffer.data())[i];
            if (font.lfCharSet == ANSI_CHARSET) {
                FontEntry entry;
                entry.recommended_weight = font.lfWeight;
                entry.type_face_name = Utils::wstring_to_string(font.lfFaceName);
                entry.flags.is_italic = font.lfItalic;
                entry.flags.is_monospace = font.lfPitchAndFamily & (FIXED_PITCH | MONO_FONT);
                entry.file_path = path_str;
                installed_fonts[path_str] = entry;
            }
        }
    }
}

void Gui::native_init() const {
    const auto hwnd = glfwGetWin32Window(glfw_window);
    if (run_as_borderless) {
        s_old_wndproc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(borderless_wnd_proc_handler)));

        constexpr MARGINS borderless = {1, 1, 1, 1};
        if (DwmExtendFrameIntoClientArea(hwnd, &borderless)) {
            LOG_WARN("Failed to extend frame into client area");
        }

        constexpr DWORD attribute = DWMNCRP_ENABLED;
        if (DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &attribute, sizeof(attribute))) {
            LOG_WARN("Failed to set DWMNCRP_ENABLED");
        }

        OSVERSIONINFOW version_info{.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW)};
        RtlGetVersion(&version_info);

        const auto is_win11 = version_info.dwBuildNumber >= 20000;
        LOG_INFO("Windows version: {}.{}.{}", is_win11 ? 11 : version_info.dwMajorVersion, version_info.dwMinorVersion,
                 version_info.dwBuildNumber);

        ACCENT_POLICY accent = {is_win11 ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_ENABLE_BLURBEHIND, 0, 0x20000000, 0};

        WINDOWCOMPOSITIONATTRIBDATA data{};
        data.dwAttrib = WCA_ACCENT_POLICY;
        data.pvData = &accent;
        data.cbData = sizeof(accent);
        SetWindowCompositionAttribute(hwnd, &data);

        if (!SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                          SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE)) {
            LOG_WARN("Failed to recalculate window parameters");
        }
    }
    else {
        s_old_wndproc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(normal_wnd_proc_handler)));
    }
}

void Gui::native_end() const {
    const auto hwnd = glfwGetWin32Window(glfw_window);
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_old_wndproc));
}

#endif
