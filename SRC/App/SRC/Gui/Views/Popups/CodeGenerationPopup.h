#pragma once
#include "PopupView.h"

#include <set>
#include <list>

#include <TextEditor.h>

class CodeGenerationPopup : public PopupView {
    using DependencyMap = std::map<std::string, std::set<std::string>>;

    DependencyMap m_dependencies{};

    std::map<std::string, bool> m_visited_structs{};

    std::list<std::string> m_sorted_structs{};

    std::unique_ptr<TextEditor> m_text_editor{};

    int m_selected_language = 0;

    std::stringstream m_code{};

    std::unordered_set<std::string> m_inserted_keywords{};
    std::unordered_set<std::string> m_inserted_identifiers{};

    void clear() {
        m_code = {};
        m_visited_structs.clear();
        m_sorted_structs.clear();
        m_dependencies.clear();

        for (auto& keyword : m_inserted_keywords) {
            auto& def = m_text_editor->GetCurrentLanguageDefinition();
            def.mKeywords.erase(keyword);
        }

        for (auto& identifier : m_inserted_identifiers) {
            auto& def = m_text_editor->GetCurrentLanguageDefinition();
            def.mIdentifiers.erase(identifier);
        }

        m_inserted_keywords.clear();
        m_inserted_identifiers.clear();
    }

    void register_keyword(const std::string &keyword) {
        if (!m_inserted_keywords.contains(keyword)) {
            auto& def = m_text_editor->GetCurrentLanguageDefinition();
            def.mKeywords.insert(keyword);
        }
    }

    void register_identifier(const std::string &identifier) {
        if (!m_inserted_identifiers.contains(identifier)) {
            auto& def = m_text_editor->GetCurrentLanguageDefinition();
            TextEditor::Identifier ident{};
            ident.mDeclaration = "Built-in type";
            def.mIdentifiers.insert({identifier, ident});
            m_inserted_identifiers.insert(identifier);
        }
    }

    void generate_cpp_code();

    void generate_rust_code();

    void build_dependencies();

    void build_sorted_structs();

    void generate_code();
public:
    CodeGenerationPopup();

    void render() override;

    bool on_close(bool after_fade) override;
    void sort_topologically(const std::string &name);
};

REGISTER_POPUP(CodeGenerationPopup);
