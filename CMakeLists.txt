cmake_minimum_required(VERSION 3.28)
project(structshaper C CXX ASM_NASM)

set(CMAKE_CXX_STANDARD 23)

if (CMAKE_VERSION EQUAL "3.28")
    message("[!] Applying hotfix for CMake 3.28")
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

# Developer mode
add_compile_definitions(STRUCTSHAPER_DEV)

# Options
option(STRUCTSHAPER_USE_SYSTEM_LIBRARIES "Searches the used external libraries on the system instead of using them from the local lib folder" ON)

# External libraries
set(LIBRARY_NAMES fmt)

if (STRUCTSHAPER_USE_SYSTEM_LIBRARIES)
    list(APPEND LIBRARY_NAMES Freetype glfw3)
else ()
    list(APPEND LIBRARY_NAMES freetype glfw)
endif ()

list(APPEND LIBRARY_NAMES nlohmann_json)

# Detect compiler
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(STRUCTSHAPER_IS_GCC TRUE)
    add_compile_definitions(STRUCTSHAPER_IS_GCC)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(STRUCTSHAPER_IS_CLANG TRUE)
    add_compile_definitions(STRUCTSHAPER_IS_CLANG)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(STRUCTSHAPER_IS_MSVC TRUE)
    add_compile_definitions(STRUCTSHAPER_IS_MSVC)
endif ()

# Detect platform
# Native specific settings
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions("STRUCTSHAPER_IS_LINUX")
    set(STRUCTSHAPER_IS_LINUX true)
    add_compile_options(-fPIC)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions("STRUCTSHAPER_IS_WINDOWS")
    set(STRUCTSHAPER_IS_WINDOWS true)
    add_compile_definitions(NOMINMAX)
endif ()

# Other definitions
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(STRUCTSHAPER_BUILD_IS_64)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(STRUCTSHAPER_BUILD_IS_86)
endif ()

# Output paths
set(STRUCTSHAPER_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/Bin/${CMAKE_C_COMPILER_ID}/${CMAKE_BUILD_TYPE})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${STRUCTSHAPER_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${STRUCTSHAPER_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${STRUCTSHAPER_OUTPUT_DIR})

# Handle external libraries
if (STRUCTSHAPER_USE_SYSTEM_LIBRARIES)
    message("[!] Using system libraries")
    foreach (LIBRARY_NAME ${LIBRARY_NAMES})
        message("[!] Searching for ${LIBRARY_NAME}")
        find_package(${LIBRARY_NAME} REQUIRED)
        if (${LIBRARY_NAME}_FOUND)
            message("[+] Found ${LIBRARY_NAME}")
            include_directories(${${LIBRARY_NAME}_INCLUDE_DIRS})
            link_libraries(${${LIBRARY_NAME}_LIBRARIES})
            link_libraries(${LIBRARY_NAME})
        else ()
            message("[-] Could not find ${LIBRARY_NAME}")
        endif ()
    endforeach ()
else ()
    message("[!] Using local libraries")

    # Custom local library options

    # Disable HarfBuzz in FreeType
    set(FT_DISABLE_HARFBUZZ ON)

    foreach (LIBRARY_NAME ${LIBRARY_NAMES})
        message("[!] Adding ${LIBRARY_NAME}")

        if (NOT TARGET ${LIBRARY_NAME})
            add_subdirectory(Extern/${LIBRARY_NAME})
            include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Extern/${LIBRARY_NAME}/include)
            link_libraries(${LIBRARY_NAME})

            # Check if the target is an INTERFACE target
            get_target_property(IS_INTERFACE ${LIBRARY_NAME} TYPE)
            if (IS_INTERFACE STREQUAL "INTERFACE_LIBRARY")
                message("[!] ${LIBRARY_NAME} is an INTERFACE target")
                target_compile_options(${LIBRARY_NAME}
                        INTERFACE
                        $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
                        $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)
            else ()
                message("[!] ${LIBRARY_NAME} is not an INTERFACE target")
                target_compile_options(${LIBRARY_NAME}
                        PRIVATE
                        $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
                        $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)
            endif ()

            message("[+] Added ${LIBRARY_NAME}")
        endif ()
    endforeach ()
endif ()

# Special case for ImGui
if (NOT TARGET imgui)
    add_compile_definitions(IMGUI_ENABLE_FREETYPE IMGUI_DEFINE_MATH_OPERATORS IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_OBSOLETE_KEYIO)
    add_subdirectory(Extern/imgui)
    include_directories(Extern/imgui/src/imgui)
    include_directories(Extern/imgui/ImGuiColorTextEdit)
    link_libraries(imgui)

    target_compile_options(imgui
            PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
            $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)

    message("[+] Added ImGui")
endif ()

# Special case for lunasvg
if (NOT TARGET lunasvg)
    add_subdirectory(Extern/lunasvg)
    include_directories(Extern/lunasvg/include)
    link_libraries(lunasvg)

    target_compile_options(lunasvg
            INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
            $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)

    message("[+] Added lunasvg")
endif ()

# OS Specific stuff
if (STRUCTSHAPER_IS_WINDOWS)
    # Link libraries
    link_libraries(ntdll dbghelp wtsapi32 dwmapi shlwapi Opengl32)

    # Needed to get color diagnostics on clang
    if (STRUCTSHAPER_IS_CLANG)
        message("[!] Enabling color diagnostics")
        set(CMAKE_COLOR_DIAGNOSTICS ON)
        list(APPEND COMPILE_FLAGS -fdiagnostics-color -fansi-escape-codes)
    elseif (STRUCTSHAPER_IS_MSVC) # MSVC
        message("[!] Enabling preprocessor for MSVC")
        list(APPEND COMPILE_FLAGS /Zc:preprocessor)
    endif ()

    if (CMAKE_BUILD_TYPE MATCHES Debug)
        message("[!] Defining _DEBUG for Windows")
        add_compile_definitions(_DEBUG)
    endif ()
elseif (STRUCTSHAPER_IS_LINUX)
    # Required for shared libraries
    add_compile_options(-fPIC)

    # OpenGL is required on Linux
    find_package(OpenGL REQUIRED)
    include_directories(${OpenGL_INCLUDE_DIRS})

    # Link libraries
    link_libraries(${OpenGL_LIBRARIES} OpenGL::OpenGL X11 Xrandr fontconfig)
endif ()

# Optimization flags
if (STRUCTSHAPER_IS_CLANG OR STRUCTSHAPER_IS_GCC)
    message("[!] Adding optimization flags for clang/gcc")
    list(APPEND COMPILE_FLAGS -msse4.2 -mcx16 -mpopcnt -msahf -mfxsr -mpclmul)

    if (CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        message("[!] Adding -O2 for clang/gcc")
        list(APPEND COMPILE_FLAGS -O2)
    endif ()

    if (CMAKE_BUILD_TYPE MATCHES Release)
        message("[!] Adding LTO for clang/gcc")
        list(APPEND COMPILE_FLAGS -flto)
    endif ()
elseif (STRUCTSHAPER_IS_MSVC)
    message("[!] Adding optimization flags for MSVC")

    if (CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        message("[!] Adding /O2 for MSVC")
        list(APPEND COMPILE_FLAGS /O2)
    endif ()

    if (CMAKE_BUILD_TYPE MATCHES Release)
        message("[!] Adding LTO for MSVC")
        list(APPEND COMPILE_FLAGS /LTCG)
    endif ()
endif ()

# Shared
add_subdirectory(SRC/Shared)
include_directories(SRC/Shared/SRC)
link_libraries(Shared)

# Subprojects
add_subdirectory(SRC/ProcessInterface)
include_directories(SRC/ProcessInterface/ProcessInterfaceBase/SRC)

add_subdirectory(SRC/App)
