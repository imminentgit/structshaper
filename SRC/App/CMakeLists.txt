cmake_minimum_required(VERSION 3.28)
project(App)

include_directories(./SRC)

add_executable(App
        SRC/main.cpp

        SRC/Gui/Gui.cpp
        SRC/Gui/Gui.h

        SRC/Gui/Native/Windows.cpp

        SRC/Gui/IconsLoader/IconsLoader.cpp
        SRC/Gui/IconsLoader/IconsLoader.h

        SRC/Gui/ImGuiHelpers/ImGuiHelpers.cpp
        SRC/Gui/ImGuiHelpers/ImGuiHelpers.h

        SRC/Queue/Event.h

        SRC/Gui/Views/BaseView.h

        SRC/Gui/Views/ProjectView.cpp
        SRC/Gui/Views/ProjectView.h

        SRC/Gui/Views/BaseView.cpp

        SRC/Gui/Views/Popups/PopupView.h
        SRC/Gui/Views/Popups/PopupView.cpp

        SRC/Gui/Views/Popups/CreateProjectPopup.cpp
        SRC/Gui/Views/Popups/CreateProjectPopup.h

        SRC/Gui/Native/Linux.cpp

        SRC/ProjectManager/ProjectManager.cpp
        SRC/ProjectManager/ProjectManager.h

        SRC/Gui/Views/Popups/MessageBoxPopup.cpp
        SRC/Gui/Views/Popups/MessageBoxPopup.h

        SRC/Gui/Views/Popups/SelectProcessPopup.cpp
        SRC/Gui/Views/Popups/SelectProcessPopup.h

        SRC/Gui/Views/StructsListView.cpp
        SRC/Gui/Views/StructsListView.h

        SRC/ProjectManager/Struct.cpp
        SRC/ProjectManager/Struct.h

        SRC/Gui/Views/ProjectBackgroundView.cpp
        SRC/Gui/Views/ProjectBackgroundView.h

        SRC/Gui/Views/Popups/CreateStructPopup.cpp
        SRC/Gui/Views/Popups/CreateStructPopup.h

        SRC/Gui/ImGuiHelpers/Formatters.h

        SRC/Gui/Views/StructView.cpp
        SRC/Gui/Views/StructView.h

        SRC/ProcessInterface/ProcessInterface.cpp
        SRC/ProcessInterface/ProcessInterface.h

        SRC/Gui/Views/Popups/SelectProcessInterfacePopup.cpp
        SRC/Gui/Views/Popups/SelectProcessInterfacePopup.h

        SRC/ProjectManager/Types/FieldBase.cpp
        SRC/ProjectManager/Types/FieldBase.h

        SRC/ProjectManager/Types/PodField.cpp
        SRC/ProjectManager/Types/PodField.h

        SRC/Gui/Views/Popups/SettingsPopup.cpp
        SRC/Gui/Views/Popups/SettingsPopup.h

        SRC/OptionsManager/OptionsManager.cpp
        SRC/OptionsManager/OptionsManager.h

        SRC/Gui/Views/MessagesView.cpp
        SRC/Gui/Views/MessagesView.h

        SRC/Gui/Views/Popups/CodeGenerationPopup.cpp
        SRC/Gui/Views/Popups/CodeGenerationPopup.h

        SRC/ProjectManager/Types/VecField.cpp
        SRC/ProjectManager/Types/VecField.h
        
        SRC/ProjectManager/Types/StructField.cpp
        SRC/ProjectManager/Types/StructField.h
)

target_compile_options(App
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
        $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)