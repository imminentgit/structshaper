#include "OptionsManager.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include <Gui/Gui.h>

KeyBindEntry &KeyBindEntry::get(const EKeyBind key_bind) {
    auto &options_manager = OptionsManager::the();
    return options_manager.options.key_binds[static_cast<size_t>(key_bind)];
}

bool KeyBindEntry::are_all_keys_pressed() {
    if (is_selecting) {
        return false;
    }

    if (already_invoked) {
        const auto was_any_key_released =
            std::ranges::any_of(keys, [](const ImGuiKey key) { return !ImGui::IsKeyDown(key); });

        if (was_any_key_released) {
            already_invoked = false;
        }

        return false;
    }

    const auto all_keys_pressed = std::ranges::all_of(keys, [](const ImGuiKey key) { return ImGui::IsKeyDown(key); });

    if (all_keys_pressed) {
        already_invoked = true;
    }

    return all_keys_pressed;
}

Options::Options() {
    const auto fill_types = [](pod_type_arr_t &arr) {
        for (size_t i = 0; i < arr.size(); ++i) {
            arr[i] = to_string(static_cast<EType>(i));
        }
    };

    fill_types(ui_pod_types);
    fill_types(field_pod_types);

    key_binds = {
        KeyBindEntry("Save Project", {ImGuiKey_LeftCtrl, ImGuiKey_S}),
        KeyBindEntry("Close Project", {ImGuiKey_LeftCtrl, ImGuiKey_Q}),
    };

    colors = {{ImColor(255, 0, 0, 255), ImColor(0, 255, 0, 255), ImColor(255, 255, 255, 255),
               ImColor(168, 50, 153, 255), ImColor(255, 255, 255, 255), ImColor(0, 0, 255, 255),
               ImColor(255, 255, 255, 255), ImColor(0, 255, 0, 255), ImColor(255, 116, 23, 255),
               ImColor(128, 128, 128, 255)}};
}

void Options::serialize(nlohmann::json &json) const {
    json["field_pod_types"] = field_pod_types;
    json["selected_font_path"] = selected_monospace_font_path;
    json["selected_font_size"] = selected_monospace_font_size;

    nlohmann::json colors_json{};
    for (const auto color : colors) {
        colors_json.push_back({static_cast<uint8_t>(color.x * 255.f), static_cast<uint8_t>(color.y * 255.f),
                               static_cast<uint8_t>(color.z * 255.f), static_cast<uint8_t>(color.w * 255.f)});
    }
    json["colors"] = colors_json;
}

void Options::deserialize(const nlohmann::json &json) {
    const auto _field_pod_types = json.find("field_pod_types");
    if (_field_pod_types != json.end()) {
        field_pod_types = _field_pod_types->get<pod_type_arr_t>();
    }

    const auto _selected_font_path = json.find("selected_font_path");
    if (_selected_font_path != json.end()) {
        selected_monospace_font_path = _selected_font_path->get<std::string>();
    }

    const auto _selected_font_size = json.find("selected_font_size");
    if (_selected_font_size != json.end()) {
        selected_monospace_font_size = _selected_font_size->get<int>();
    }

    const auto _colors = json.find("colors");
    if (_colors != json.end()) {
        auto &json_colors = _colors.value();
        for (size_t i = 0; i < json_colors.size(); ++i) {
            auto json_col_arr = json_colors[i].get<std::array<uint8_t, 4>>();
            colors[i] = ImColor(json_col_arr[0], json_col_arr[1], json_col_arr[2], json_col_arr[3]);
        }
    }
}

void OptionsManager::init() {
    if (!std::filesystem::exists(SETTINGS_PATH)) {
        save();
        return;
    }

    std::ifstream file(SETTINGS_PATH);
    if (!file.is_open()) {
        return;
    }

    options.deserialize(nlohmann::json::parse(file));
}

void OptionsManager::save() const {
    nlohmann::json json{};
    options.serialize(json);

    std::ofstream file(SETTINGS_PATH);
    file << json.dump(4);
}
