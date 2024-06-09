#include "IconsLoader.h"

#include <filesystem>
#include <unordered_map>

#include <Gui/Gui.h>

#include <Logger/Logger.h>

namespace IconsLoader {
    std::unordered_map<std::string, LunaDocument> s_icons{};

    std::unordered_map<Utils::Fnv1::HashType, GLuint> s_rasterized_icons{};

    inline Utils::Fnv1::HashType hash_icon_size(const std::string_view icon_name, const uint32_t width, const uint32_t height) {
        return Utils::Fnv1::hash(icon_name) ^ width << 16 ^ height;
    }

    LunaDocument::pointer get_icon(const std::string_view icon_name) {
        const auto icon = s_icons.find({icon_name.data(), icon_name.size()});
        if (icon == s_icons.end()) {
            LOG_WARN("Icon not found: {}", icon_name);
            return nullptr;
        }

        return icon->second.get();
    }

    GLuint get_icon_texture(const std::string_view icon_name, const uint32_t width, const uint32_t height) {
        const auto icon = s_icons.find({icon_name.data(), icon_name.size()});
        if (icon == s_icons.end()) {
            LOG_WARN("Icon not found: {}", icon_name);
            return 0;
        }

        const auto icon_size_hash = hash_icon_size(icon_name, width, height);
        const auto icon_texture = s_rasterized_icons.find(icon_size_hash);
        if (icon_texture != s_rasterized_icons.end()) {
            return icon_texture->second;
        }

        auto bitmap = icon->second->renderToBitmap(width, height);
        bitmap.convertToRGBA();

        const auto texture = Gui::the().create_texture(bitmap.width(), bitmap.height());
        Gui::update_texture_rgba(texture, bitmap.width(), bitmap.height(), bitmap.data());
        LOG_INFO("Icon rasterized: {} ({}x{})", icon_name, width, height);

        s_rasterized_icons[icon_size_hash] = texture;
        return texture;
    }

    void init() {
        std::string_view icons_path =
#ifdef STRUCTSHAPER_DEV
                "../../../Data/Icons";
#else
        "Data/Icons";
#endif
        if (!std::filesystem::exists(icons_path)) {
            LOG_WARN("Icons path does not exist: {}", icons_path);
            return;
        }

        for (const auto &entry: std::filesystem::directory_iterator(icons_path)) {
            const auto &path = entry.path();
            const auto path_str = path.string();
            if (!entry.is_regular_file() || entry.path().extension() != ".svg") {
                LOG_WARN("Skipping non-svg file: {}", path_str);
                continue;
            }

            s_icons[path.stem().string()] = lunasvg::Document::loadFromFile(path_str);
        }
    }
}