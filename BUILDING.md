# BUILDING

### Requirements:

- A modern verison of Clang/GCC, MSVC is not supported.

### Dependencies:

- [glfw](https://www.glfw.org/)
- [fmt](https://github.com/fmtlib/fmt)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [LunaSVG](https://github.com/sammycage/lunasvg)
- [freetype](http://freetype.org/)
- [nlohmann_json](https://github.com/nlohmann/json)

### Other:

The used icons are [IntelliJ Platform Icons](https://intellij-icons.jetbrains.design/)

### CMake Build Options:

| STRUCTSHAPER_USE_SYSTEM_LIBRARIES | Searches the used external libraries on the system instead of using them from the local extern folder | ON/OFF |
| --------------------------------- | ----------------------------------------------------------------------------------------------------- | ------ |
| STRUCTSHAPER_USE_DEV_MODE         | Moves the resource directories into a absolute path and enables some debug features.                  | ON/OFF |

### Actual Building:

1. Clone latest source using git.
2. Make a build directory and cd to it.
3. Run ```cmake (INSERT YOUR BUILD OPTIONS) ..```.
4. Build using your preferred build tool.