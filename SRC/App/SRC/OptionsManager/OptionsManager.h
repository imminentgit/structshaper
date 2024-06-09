#pragma once
#include <array>
#include <set>
#include <string>
#include <utility>
#include <variant>

#include <ProjectManager/Types/PodField.h>

#include <Utils/StaticInstance.h>

enum class EColorIndex {
    OFFSET_COLOR,
    ADDRESS_COLOR,
    UNNAMED_TYPE_COLOR,
    NAMED_TYPE_COLOR,
    FIELD_NAME_COLOR,
    ASCII_VIEW_COLOR,
    HEX_VIEW_COLOR,
    SEPARATOR_COLOR,
    FLOAT_VALUE_COLOR,
    POD_TYPE_VIEW_COLOR,
    MAX
};

static const char* to_string(const EColorIndex index) {
    switch (index) {
    case EColorIndex::OFFSET_COLOR: return "Offset";
    case EColorIndex::ADDRESS_COLOR: return "Address";
    case EColorIndex::UNNAMED_TYPE_COLOR: return "Unnamed type";
    case EColorIndex::NAMED_TYPE_COLOR: return "Named type";
    case EColorIndex::FIELD_NAME_COLOR: return "Field name";
    case EColorIndex::ASCII_VIEW_COLOR: return "ASCII view";
    case EColorIndex::HEX_VIEW_COLOR: return "Hex view";
    case EColorIndex::SEPARATOR_COLOR: return "Separator";
    case EColorIndex::FLOAT_VALUE_COLOR: return "Float value";
    case EColorIndex::POD_TYPE_VIEW_COLOR: return "POD type view";
    default: return "Unknown";
    }
}

enum class EKeyBind { SAVE, CLOSE_PROJECT, MAX };

struct KeyBindEntry {
    using KeySetType = std::set<ImGuiKey>;

    std::string display_name{};
    KeySetType default_keys{};
    KeySetType keys{};

    bool already_invoked{};
    bool is_selecting{};

    KeyBindEntry() = default;
    KeyBindEntry(std::string display_name, const KeySetType& default_keys) :
        display_name(std::move(display_name)), default_keys(default_keys), keys(default_keys) {}

    static KeyBindEntry& get(EKeyBind key_bind);

    [[nodiscard]] std::string to_string() const {
        std::string out{};
        for (const auto key : keys) {
            out += ImGui::GetKeyName(key);

            if (key != *keys.rbegin()) {
                out += " + ";
            }
        }
        return out;
    }

    [[nodiscard]] bool are_all_keys_pressed();

    [[nodiscard]] bool is_any_key_pressed() const {
        if (is_selecting) {
            return false;
        }

        return std::ranges::any_of(keys, [](const ImGuiKey key) {
            return ImGui::IsKeyPressed(key);
        });
    }
};

struct Options {
    using pod_type_arr_t = std::array<std::string, static_cast<size_t>(EType::COUNT_ALL_TYPES)>;

    pod_type_arr_t ui_pod_types{};
    pod_type_arr_t field_pod_types{};

    std::string selected_monospace_font_path{};
    int selected_monospace_font_size = 14;

    std::array<KeyBindEntry, static_cast<size_t>(EKeyBind::MAX)> key_binds{};

    std::array<ImVec4, static_cast<size_t>(EColorIndex::MAX)> colors{};

    Options();

    void serialize(nlohmann::json &json) const;
    void deserialize(const nlohmann::json &json);
};

struct OptionsManager : StaticInstance<OptionsManager> {
    inline static auto SETTINGS_PATH =
#ifdef STRUCTSHAPER_DEV
        #ifdef STRUCTSHAPER_IS_WINDOWS
            "../../../Data/settings_win.json";
        #else
            "../../../Data/settings_linux.json";
        #endif
#else
        #ifdef STRUCTSHAPER_IS_WINDOWS
            "Data/settings_win.json";
        #else
            "Data/settings_linux.json";
        #endif
#endif

    Options options{};
    std::unordered_map<std::string, ImFont *> fonts_list{};

    void init();

    void save() const;
};