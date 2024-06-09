#include "CodeGenerationPopup.h"

#include <ProjectManager/Types/StructField.h>

#include <OptionsManager/OptionsManager.h>

#include <Logger/Logger.h>

#include <Queue/Event.h>

CodeGenerationPopup::CodeGenerationPopup() : PopupView("Code generation") {
    size_behavior.HALF_MAIN_WINDOW_X = size_behavior.HALF_MAIN_WINDOW_Y = true;
    scrolling = false;

    Event::OnMenuBarRender::listen(
        [] {
            if (ImGui::MenuItem("Generate code")) {
                show_popup(popup_CodeGenerationPopup);
            }
        },
        2);
}

void CodeGenerationPopup::build_dependencies() {
    auto& proj_manager = ProjectManager::the();
    auto& structs = proj_manager.structs;

    for (auto& [struct_name, struct_entry] : structs) {
        for (auto it = struct_entry.range_begin(); it != struct_entry.range_end(); ++it) {
            const auto& field = *it;

            if (field->class_type == EFieldClassType::StructField) {
                auto struct_field = std::dynamic_pointer_cast<StructField>(field);
                const auto* other_struct = struct_field->obtain_other_struct_data();
                if (other_struct) {
                    m_dependencies[struct_name].insert(other_struct->name);
                }
                else {
                    LOG_WARN("Failed to obtain other struct data for field: {}", field->name);
                }
            }
        }
    }
}

void CodeGenerationPopup::sort_topologically(const std::string& name) {
    m_visited_structs[name] = true;

    const auto& dependencies = m_dependencies[name];
    for (const auto& dependency : dependencies) {
        const auto is_visited = m_visited_structs[dependency];
        if (!is_visited) {
            sort_topologically(dependency);
        }
    }

    m_sorted_structs.emplace_front(name);
    register_identifier(name);
}

void CodeGenerationPopup::build_sorted_structs() {
    auto& proj_manager = ProjectManager::the();
    auto& structs = proj_manager.structs;

    build_dependencies();
    for (const auto& name: structs | std::views::keys) {
        const auto is_visited = m_visited_structs[name];
        if (!is_visited) {
            sort_topologically(name);
        }
    }
}

void CodeGenerationPopup::generate_cpp_code() {
    auto& proj_manager = ProjectManager::the();
    auto& structs = proj_manager.structs;

    m_code << "#pragma once\n\n";

    for (const auto& struct_name : m_sorted_structs | std::views::reverse) {
        auto& struct_data = structs[struct_name];
        m_code << fmt::format("struct {} {{\n" ,struct_name);

        size_t padding_count{};
        for (auto it = struct_data.range_begin(); it != struct_data.range_end(); ++it) {
            const auto& field = *it;

            if (field->state == EFieldState::UNNAMED) {
                padding_count += field->memory_size();
                continue;
            }

            if (padding_count != 0) {
                m_code << fmt::format("    uint8_t padding_0x{:X}[0x{:X}];\n", field->offset, padding_count);
                padding_count = 0;
            }

            const auto type_name = field->type_name();
            register_identifier(type_name);

            m_code << fmt::format("    {} {};\n", type_name, field->name);
        }

        m_code << "};\n\n";
    }
}

void CodeGenerationPopup::generate_rust_code() {
    auto& proj_manager = ProjectManager::the();
    auto& structs = proj_manager.structs;

    m_code << "#![allow(dead_code)]\n\n\n";

    for (const auto& struct_name : m_sorted_structs | std::views::reverse) {
        auto& struct_data = structs[struct_name];
        m_code << fmt::format("#[repr(C)]\nstruct {} {{\n" ,struct_name);

        size_t padding_count{};
        for (auto it = struct_data.range_begin(); it != struct_data.range_end(); ++it) {
            const auto& field = *it;

            if (field->state == EFieldState::UNNAMED) {
                padding_count += field->memory_size();
                continue;
            }

            if (padding_count != 0) {
                m_code << fmt::format("    padding_0x{:X}: [u8; 0x{:X}],\n", field->offset, padding_count);
                padding_count = 0;
            }

            const auto type_name = field->type_name();
            register_identifier(type_name);

            m_code << fmt::format("    {}: {},\n", field->name, type_name);
        }

        m_code << "};\n\n";
    }
}

void CodeGenerationPopup::generate_code() {
    clear();

    auto& option_mgr = OptionsManager::the();
    auto& types = option_mgr.options.ui_pod_types;

    for (const auto& keyword : types) {
        register_keyword(keyword);
    }

    build_sorted_structs();

    m_text_editor->SetText({});

    m_selected_language == 0 ? generate_cpp_code() : generate_rust_code();

    m_text_editor->SetText(m_code.str());
}

void CodeGenerationPopup::render() {
    using namespace ImGui::Helpers;

    PopupView::render();

    if (!m_text_editor) {
        m_text_editor = std::make_unique<TextEditor>();
        m_text_editor->SetLanguageDefinition(TextEditor::LanguageDefinitionId::Rust);
        m_text_editor->SetPalette(TextEditor::PaletteId::Dark);
        m_text_editor->SetReadOnlyEnabled(true);
        m_text_editor->SetShowLineNumbersEnabled(false);

        if (m_text_editor) {
            generate_code();
        }
    }

    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::Combo("##Language", &m_selected_language, "C++\0Rust\0")) {
        m_selected_language == 0 ? m_text_editor->SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cpp)
                                 : m_text_editor->SetLanguageDefinition(TextEditor::LanguageDefinitionId::Rust);

        generate_code();
    }

    const auto &gui = Gui::the();
    ImGui::PushFont(gui.monospace_font);
    m_text_editor->Render("##Code");
    ImGui::PopFont();
}

bool CodeGenerationPopup::on_close(const bool after_fade) {
    if (after_fade) {
        clear();
    }
    return true;
}
