cmake_minimum_required(VERSION 3.28)
project(imgui)

# Freetype
include_directories(${FREETYPE_INCLUDE_DIRS} ${glfw3_INCLUDE_DIRS} src/imgui ImGuiColorTextEdit)
link_libraries(freetype glfw)

add_library(imgui STATIC
        src/imgui/imgui.cpp
        src/imgui/imgui.h
        src/imgui/imgui_demo.cpp
        src/imgui/imgui_draw.cpp
        src/imgui/imgui_internal.h
        src/imgui/imgui_tables.cpp
        src/imgui/imgui_widgets.cpp
        src/imgui/imstb_rectpack.h
        src/imgui/imstb_textedit.h
        src/imgui/imstb_truetype.h

        src/imgui/misc/cpp/imgui_stdlib.cpp
        src/imgui/misc/cpp/imgui_stdlib.h

        src/imgui/misc/freetype/imgui_freetype.cpp
        src/imgui/misc/freetype/imgui_freetype.h

        src/imgui/backends/imgui_impl_opengl3.cpp
        src/imgui/backends/imgui_impl_opengl3.h
        src/imgui/backends/imgui_impl_opengl3_loader.h

        src/imgui/backends/imgui_impl_glfw.cpp
        src/imgui/backends/imgui_impl_glfw.h

        ImGuiColorTextEdit/TextEditor.cpp
        ImGuiColorTextEdit/TextEditor.h

        ImGuiColorTextEdit/LanguageDefinitions.cpp
        ImGuiColorTextEdit/ImGuiDebugPanel.cpp
)