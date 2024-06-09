#include "StructField.h"

#include <Gui/ImGuiHelpers/ImGuiHelpers.h>

#include <Queue/Event.h>

#include <Utils/ScopeGuard.h>

Struct *StructField::obtain_other_struct_data() const {
    auto &project_manager = ProjectManager::the();
    auto &structs = project_manager.structs;
    if (!structs.contains(m_other_struct_name)) {
        return nullptr;
    }

    return &structs.at(m_other_struct_name);
}

void StructField::update_fields() {
    auto other_struct_data = obtain_other_struct_data();
    if (!other_struct_data) {
        return;
    }

    const auto begin = other_struct_data->range_begin();
    const auto end = other_struct_data->range_end();
    if (begin == end) {
        return;
    }

    m_memory_size = 0;

    size_t i = 0;
    for (auto it = begin; it != end; ++it) {
        auto &field = *it;
        if (field->state == EFieldState::NAMED) {
            m_memory_size += field->memory_size();
            if (m_fields.contains(field->id)) {
                continue;
            }

            auto cloned = field->clone();
            // Looks weird, but this to make sure the id is unique
            cloned->init_base(parent, field->id + id + field->offset + i);
            cloned->address = address + field->offset;
            cloned->offset = field->offset;
            cloned->is_dummy = true;

            m_fields[field->id] = std::move(cloned);
        }

        ++i;
    }
}

StructField::StructField(std::string other_struct_name) :
    FieldBase(EFieldClassType::StructField, EType::STRUCT_SPECIAL_TYPE),
    m_other_struct_name(std::move(other_struct_name)) {

    m_rename_event_idx = Event::StructRename::listen([this](const std::string &old_name, const std::string &new_name) {
        if (m_other_struct_name == old_name) {
            m_other_struct_name = new_name;
            return true;
        }

        return false;
    });

    m_event_update_idx = Event::OnStructUpdate::listen([this](const std::string &struct_name) {
        if (m_other_struct_name == struct_name) {
            update_fields();
            return true;
        }

        return false;
    });

    m_field_removed_idx = Event::RemoveField::listen([this](const std::string &struct_name, UniversalId field_id) {
        if (m_other_struct_name == struct_name && m_fields.contains(field_id)) {
            m_fields.erase(field_id);
            return true;
        }

        return false;
    });
}

StructField::~StructField() {
    Event::StructRename::unlisten(m_rename_event_idx);
    Event::OnStructUpdate::unlisten(m_event_update_idx);
    Event::RemoveField::unlisten(m_field_removed_idx);
}

void StructField::draw_fields() {
    if (!m_did_first_update) {
        update_fields();
        m_did_first_update = true;
    }

    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

    const auto tree_open = ImGui::TreeNodeEx(make_imgui_id("_OtherStructTree"),
                                             ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

    // Needs to be reset after the tree node
    render_data.x_offset = 0;

    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::BeginGroup();

    render_field_info();
    render_field_name(true, false);
    ImGui::PopStyleVar();

    ImGui::SameLine(render_data.x_offset);
    ImGui::TextUnformatted(";");

    ImGui::EndGroup();

    if (!tree_open) {
        return;
    }
    ScopeGuard tree_guard(&ImGui::TreePop);

    const auto &style = ImGui::GetStyle();
    for (auto &[id, field] : m_fields) {
        field->address = address + field->offset;
        field->render(false);
    }
}

void StructField::render(bool is_offscreen) {
    using namespace ImGui::Helpers;

    render_begin(true);

    switch (state) {
    case EFieldState::UNNAMED:
        render_field_hex_info();
        render_field_float_info();
        break;
    case EFieldState::NAMED:
        draw_fields();
        break;
    case EFieldState::EDITING_NAME:
        render_field_info();
        render_field_name(false);
        ImGui::PopStyleVar();
        break;
    }

    render_end(is_offscreen);
}

size_t StructField::memory_size() {
    return m_memory_size;
}

std::string StructField::type_name() { return m_other_struct_name; }

void StructField::serialize(nlohmann::json &json) {
    FieldBase::serialize(json);

    json["other_struct_name"] = m_other_struct_name;
}

void StructField::deserialize(const nlohmann::json &json) {
    FieldBase::deserialize(json);

    m_other_struct_name = json["other_struct_name"];

    if (obtain_other_struct_data() == nullptr) {
        Gui::the().add_notification("Failed to load struct field", "Struct not found: " + m_other_struct_name);
        reset_field();
    }
}

void StructField::reset_field() {
    const auto new_field = PodField::make_default_pod_field();
    new_field->init_base(parent, id);
    new_field->set_default_name();

    Event::SwapField::call(parent->name, id, new_field);
}

void StructField::copy_data(std::shared_ptr<FieldBase> to_field) { FieldBase::copy_data(to_field); }
