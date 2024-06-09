#include "SettingsPopup.h"

#include <Gui/Gui.h>
#include <Gui/ImGuiHelpers/ImGuiHelpers.h>


#include <OptionsManager/OptionsManager.h>

#include <Queue/Event.h>

#include <Logger/Logger.h>

#include <Utils/ScopeGuard.h>
#include <Utils/Utils.h>

struct TabEntry {
    std::string name{};
    std::function<void()> render{};
};

static inline auto settings_tabs = Utils::array_of<TabEntry>(
    TabEntry{"Fonts",
             [] {
                 using namespace ImGui::Helpers;

                 auto &gui = Gui::the();
                 auto &options = OptionsManager::the().options;
                 auto &selected_font_entry = gui.installed_fonts.at(options.selected_monospace_font_path);

                 ImGui::TextUnformatted("Monospace font:");
                 ImGui::SetNextItemWidth(-1.f);
                 if (ImGui::BeginCombo("##OptionsFontCombo", selected_font_entry.type_face_name)) {
                     for (const auto &[file_path, entry] : gui.installed_fonts) {
                         if (!entry.flags.is_monospace || entry.type_face_name.contains("Emoji")) {
                             continue;
                         }

                         std::string selectable_str{};
                         if (entry.flags.is_italic) {
                             selectable_str =
                                 fmt::format("{} {} - Italic", entry.type_face_name, entry.recommended_weight);
                         }
                         else {
                             selectable_str = fmt::format("{} {}", entry.type_face_name, entry.recommended_weight);
                         }

                         // Issue: Preview is disabled due to imgui not supporting multiple atlases
                         //ImGui::PushFont(entry.preview_font);
                         if (ImGui::Selectable(selectable_str,
                                               options.selected_monospace_font_path == file_path)) {
                             options.selected_monospace_font_path = file_path;
                             Event::UpdateFont::call(selected_font_entry.file_path,
                                                     options.selected_monospace_font_size);
                             OptionsManager::the().save();
                         }
                         //ImGui::PopFont();
                     }

                     ImGui::EndCombo();
                 }

                 ImGui::TextUnformatted("Size:");
                 ImGui::SetNextItemWidth(-1.f);
                 if (ImGui::InputInt("##OptionsFontSize", &options.selected_monospace_font_size, 1, 1,
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
                     Event::UpdateFont::call(selected_font_entry.file_path, options.selected_monospace_font_size);
                     OptionsManager::the().save();
                 }

                 ImGui::PushFont(gui.monospace_font);
                 ImGui::TextWrapped("The quick brown fox jumps over the lazy dog");
                 ImGui::PopFont();
             }},

    TabEntry{
        "Pod Types",
        [] {
            using namespace ImGui::Helpers;

            if (!ImGui::BeginChild("##OptionsDefaultTypesList")) {
                return;
            }

            ScopeGuard child_guard(&ImGui::EndChild);

            auto &options = OptionsManager::the().options;
            auto &ui_types = options.ui_pod_types;
            auto &types = options.field_pod_types;

            for (size_t i = 0; i < ui_types.size(); ++i) {
                auto &type = ui_types[i];
                const auto type_name = to_string(static_cast<EType>(i));
                if (input_text_top_label(fmt::format("{}:", type_name), type, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (const auto verify = Utils::verify_name(type); verify != Utils::EVerifyReason::OK) {
                        Gui::the().add_notification("Failed to update pod type", Utils::to_string(verify));
                        type = to_string(static_cast<EType>(i));
                    }
                    else {
                        types[i] = type;
                        OptionsManager::the().save();
                    }
                }
            }
        }},

    TabEntry{"Keybinds",
             [] {
                 using namespace ImGui::Helpers;

                 if (!ImGui::BeginChild("##OptionsKeybindsList")) {
                     return;
                 }

                 ScopeGuard child_guard(&ImGui::EndChild);
                 auto &options = OptionsManager::the().options;

                 for (auto &key_bind : options.key_binds) {
                     ImGui::Text("%s:", key_bind.display_name.c_str());
                     ImGui::SameLine();

                     const auto button_str = key_bind.is_selecting ? "Apply" : "Edit";
                     if (aligned_win_size(
                             RIGHT,
                             [&button_str, &key_bind] {
                                 return ImGui::Button(fmt::format("{}##{}", button_str, key_bind.display_name));
                             },
                             key_bind.display_name)) {
                         if (key_bind.is_selecting) {
                             key_bind.is_selecting = false;
                             return;
                         }

                         key_bind.is_selecting = !key_bind.is_selecting;
                         key_bind.keys.clear();
                     }

                     const auto keybind_str = key_bind.to_string();
                     if (!keybind_str.empty()) {
                         ImGui::TextWrapped("%s", keybind_str.c_str());
                     }

                     ImGui::Separator();

                     if (key_bind.is_selecting) {
                         const auto exit = !ImGui::IsAnyItemFocused() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

                         for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_ReservedForModCtrl;
                              key = static_cast<ImGuiKey>(key + 1)) {

                             if (key == ImGuiKey_Escape) {
                                 continue;
                             }

                             if (exit) {
                                 key_bind.is_selecting = false;
                                 break;
                             }

                             if (ImGui::IsKeyPressed(key)) {
                                 key_bind.keys.emplace(key);
                             }
                         }
                     }
                 }
             }},
    TabEntry{"Colors",
             [] {
                 using namespace ImGui::Helpers;

                 if (!ImGui::BeginChild("##OptionsColorList")) {
                     return;
                 }

                 ScopeGuard child_guard(&ImGui::EndChild);
                 auto &options = OptionsManager::the().options;

                 for (size_t i = 0; i < options.colors.size(); ++i) {
                     auto &color = options.colors[i];

                     const auto color_name = to_string(static_cast<EColorIndex>(i));
                     const auto picker_id = fmt::format("##ColorPicker_{}", color_name);
                     ImGui::Text("%s:", color_name);

                     ImGui::SetNextItemWidth(-1.f);
                     ImGui::ColorEdit3(picker_id, &color.x);
                     ImGui::Separator();
                 }
             }},
    TabEntry{"About", [] {
                 using namespace ImGui::Helpers;

                 aligned_win_size(CENTER_BOTH, [] {
                     ImGui::BeginGroup();
                     const auto scaled_size = 32.f * Gui::the().scale_factor;
                     draw_image("class_info_color", {scaled_size, scaled_size});
                     ImGui::SameLine();
                     ImGui::PushFont(Gui::the().large_font);
                     ImGui::TextUnformatted("structshaper");
                     ImGui::PopFont();
                     ImGui::EndGroup();
                 });

                 aligned_win_size(CENTER_HORIZONTAL, [] {
                     ImGui::TextUnformatted("A tool for generating struct definitions from other programs.");
                 });
             }});

SettingsPopup::SettingsPopup() : PopupView("Settings") {
    size_behavior.HALF_MAIN_WINDOW_X = size_behavior.HALF_MAIN_WINDOW_Y = true;
    scrolling = false;

    Event::OnMenuBarRender::listen([] {
        if (ImGui::MenuItem("Settings")) {
            show_popup(popup_SettingsPopup);
        }
    }, 2);
}

void SettingsPopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();
    if (!ImGui::BeginTabBar("##OptionsTabBar")) {
        return;
    }

    ScopeGuard tabbar_guard(&ImGui::EndTabBar);

    for (const auto &tab : settings_tabs) {
        if (ImGui::BeginTabItem(tab.name)) {
            tab.render();
            ImGui::EndTabItem();
        }
    }
}

bool SettingsPopup::on_close(const bool after_fade) {
    if (after_fade) {
        auto &options_mgr = OptionsManager::the();

        for (auto &key_bind : options_mgr.options.key_binds) {
            key_bind.is_selecting = false;
        }

        options_mgr.save();
    }

    return true;
}
