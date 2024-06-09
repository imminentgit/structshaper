#include <algorithm>
#include <charconv>
#include <filesystem>
#include <functional>
#include <mutex>
#include <Gui/Gui.h>
#include <OptionsManager/OptionsManager.h>

#include <Logger/Logger.h>

void parse_args(int argc, char **argv) {
    if (argc < 2) {
        return;
    }

    std::for_each(argv + 1, argv + argc, [](char *arg_c_str) {
        auto arg = std::string_view(arg_c_str);
        if (arg.contains("borderless")) {
            Gui::the().run_as_borderless = true;
            LOG_INFO("Starting as borderless window");
        }

        constexpr std::string_view SCALE_TEXT = "-scale=";
        if (arg.starts_with(SCALE_TEXT)) {
            auto scale_str = arg.substr(SCALE_TEXT.length());
            auto &scale = Gui::the().scale_factor;
            const auto [_, ec] = std::from_chars(scale_str.data(), scale_str.data() + scale_str.size(), scale);
            if (ec != std::errc()) {
                LOG_WARN("Failed to parse scale_str factor from argument: {}", scale_str);
                scale = 1.f;
            }
        }
    });
}

void create_directories() {
    static const char *directories[] = {"Data", "Data/Icons", "Projects", "Memory_Interfaces"};
    for (const auto &dir : directories) {
#ifdef STRUCTSHAPER_DEV
        std::filesystem::create_directory(fmt::format("../../../{}", dir));
#else
        std::filesystem::create_directory(dir);
#endif
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    create_directories();

    OptionsManager::the().init();

    Gui::the().init();

    return 0;
}
