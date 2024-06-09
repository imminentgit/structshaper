#include "VecField.h"

#include "../Struct.h"
#include "Logger/Logger.h"

#include <misc/cpp/imgui_stdlib.h>

#include <Gui/Views/MessagesView.h>

#include <Gui/IconsLoader/IconsLoader.h>

#include <OptionsManager/OptionsManager.h>

#include <Utils/ScopeGuard.h>

#include <Queue/Event.h>

void VecField::render(const bool is_offscreen) {
    using namespace ImGui::Helpers;

    render_begin();

    {
        const auto &options_mgr = OptionsManager::the();
        const auto &colors = options_mgr.options.colors;

        const auto is_named = state == EFieldState::NAMED;
        if (is_named || state == EFieldState::EDITING_NAME) {
            render_field_name(is_named, true, [this] {
                if (m_are_all_the_same_type) {
                    const auto &style = ImGui::GetStyle();

                    const auto button_size = ImVec2(20.f, ImGui::GetTextLineHeight());

                    const auto icon =
                        m_collapse_type ? "arrow_expand"_svg_icon(button_size) : "arrow_collapse"_svg_icon(button_size);

                    push_dark_style_button();
                    if (title_bar_button(make_imgui_id("_TypeExpandCollapseButton"), icon, button_size)) {
                        m_collapse_type = !m_collapse_type;
                    }
                    pop_dark_style_button();

                    if (ImGui::BeginItemTooltip()) {
                        ImGui::TextUnformatted("Collapse/Expand type");
                        ImGui::EndTooltip();
                    }

                    render_data.x_offset += button_size.x + style.ItemSpacing.x;
                }
            });
            ScopeGuard guard(&ImGui::PopStyleVar, 1);

            if (is_named) {
                ImGui::PushStyleColor(ImGuiCol_Text, colors[static_cast<size_t>(EColorIndex::FIELD_NAME_COLOR)]);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

                const auto comma_w = get_char_width(',') + 1.f;
                const auto space_w = get_char_width(' ') + 1.f;

                const auto paren_begin = render_data.x_offset + get_char_width('(') + 1.f;
                ImGui::TextUnformatted("(");
                render_data.x_offset = render_data.x_begin;

                for (const auto &[i, component] : m_components | std::ranges::views::enumerate) {
                    const auto should_break = i % 4 == 0;

                    if (should_break) {
                        if (i > 0) {
                            ImGui::SameLine(render_data.x_offset);
                            ImGui::TextUnformatted(", ");
                        }
                        render_data.x_offset = paren_begin;
                        if (i != 0) {
                            ImGui::Dummy(ImVec2(0, 0));
                        }
                        ImGui::SameLine(render_data.x_offset);
                    }
                    else {
                        ImGui::SameLine(render_data.x_offset + 1.f);
                        ImGui::TextUnformatted(", ");
                        render_data.x_offset += comma_w + space_w;
                        ImGui::SameLine(render_data.x_offset);
                    }

                    const auto comp_name_text = fmt::format("{} = ", component.name);
                    auto text_rect = ImGui::CalcTextSize(comp_name_text);

                    ImGui::TextUnformatted(comp_name_text);

                    render_data.x_offset += text_rect.x + 1.f;
                    ImGui::SameLine(render_data.x_offset);

                    text_rect = ImGui::CalcTextSize(component.value);
                    ImGui::SetNextItemWidth(text_rect.x + 1.f);
                    ImGui::InputText(make_imgui_id("_{}_{}", component.name, i), &component.value);

                    if (!ImGui::IsItemFocused() && state == EFieldState::NAMED) {
                        const auto component_size = pod_type_size(component.type);

                        const auto read_value = [this, component_size, i]<typename T> -> std::string {
                            return fmt::format("{}",
                                               *reinterpret_cast<T *>(data_in_memory_buffer() + i * component_size));
                        };

                        switch (component.type) {
                        case EType::I8:
                            component.value = read_value.operator()<int8_t>();
                            break;
                        case EType::I16:
                            component.value = read_value.operator()<int16_t>();
                            break;
                        case EType::I32:
                            component.value = read_value.operator()<int32_t>();
                            break;
                        case EType::I64:
                            component.value = read_value.operator()<int64_t>();
                            break;
                        case EType::U8:
                            component.value = read_value.operator()<uint8_t>();
                            break;
                        case EType::U16:
                            component.value = read_value.operator()<uint16_t>();
                            break;
                        case EType::U32:
                            component.value = read_value.operator()<uint32_t>();
                            break;
                        case EType::U64:
                            component.value = read_value.operator()<uint64_t>();
                            break;
                        case EType::F32:
                            component.value = read_value.operator()<float>();
                            break;
                        case EType::F64:
                            component.value = read_value.operator()<double>();
                            break;
                        default:
                            MessagesView::add(
                                fmt::format(
                                    "Failed to read value on field {}, because the component type is invalid: {}", name,
                                    to_string(base_type)),
                                EMessageType::ERROR_MSG);
                            break;
                        }
                    }

                    render_data.x_offset += ImGui::GetItemRectSize().x + 1.f;
                }

                ImGui::SameLine(render_data.x_offset);
                const auto end_str = ");";
                ImGui::TextUnformatted(end_str);
                render_data.x_offset += ImGui::CalcTextSize(end_str).x + 1.f;
                ImGui::PopStyleColor(2);
            }
        }
    }

    render_end(is_offscreen);
}

size_t VecField::memory_size() { return m_memory_size; }

std::string VecField::type_name() {
    auto base_type_name = FieldBase::type_name() + "<";

    switch (base_type) {
    case EType::EULER: {
        base_type_name += to_string(static_cast<EEulerOrdering>(optional_ordering));
        base_type_name += ", ";
        break;
    }
    case EType::COLOR: {
        base_type_name += to_string(static_cast<EColorType>(optional_ordering));
        base_type_name += ", ";
        break;
    }
    case EType::MATRIX_COLUMN_MAJOR: {
        base_type_name += fmt::format("COLUMN_MAJOR, {}x{}, ", optional_cols, optional_rows);
        break;
    }
    case EType::MATRIX_ROW_MAJOR: {
        base_type_name += fmt::format("ROW_MAJOR, {}x{}, ", optional_rows, optional_cols);
        break;
    }
    default: {
        break;
    }
    }

    if (m_are_all_the_same_type && m_collapse_type) {
        base_type_name += to_string(m_components[0].type);
    }
    else {
        size_t i = 0;
        for (const auto &component : m_components) {
            base_type_name += to_string(component.type);
            if (i < m_components.size() - 1) {
                base_type_name += ", ";
            }

            ++i;
        }
    }

    return base_type_name + ">";
}

void VecField::serialize(nlohmann::json &json) {
    FieldBase::serialize(json);

    json["collapse_type"] = m_collapse_type;
    json["are_all_the_same_type"] = m_are_all_the_same_type;

    json["memory_size"] = m_memory_size;

    json["optional_ordering"] = optional_ordering;
    json["optional_rows"] = optional_rows;
    json["optional_cols"] = optional_cols;

    auto &components = json["components"];
    for (const auto &component : m_components) {
        components.push_back({{"name", component.name}, {"type", component.type}});
    }
}

size_t obtain_memory_size_loop(const VecField::ComponentVector &components) {
    size_t memory_size = 0;
    for (const auto &component : components) {
        memory_size += pod_type_size(component.type);
    }
    return memory_size;
}

void VecField::deserialize(const nlohmann::json &json) {
    FieldBase::deserialize(json);

    m_collapse_type = json.value("collapse_type", true);
    m_are_all_the_same_type = json.value("are_all_the_same_type", false);

    m_components.clear();
    for (const auto &component : json["components"]) {
        m_components.push_back({component.at("name"), component.at("type")});
    }

    m_memory_size = json.value("memory_size", obtain_memory_size_loop(m_components));

    optional_ordering = json.value("optional_ordering", 0);
    optional_rows = json.value("optional_rows", 0);
    optional_cols = json.value("optional_cols", 0);
}

void VecField::reset_field() {
    const auto new_field = PodField::make_default_pod_field();
    new_field->init_base(parent, id);
    new_field->set_default_name();

    Event::SwapField::call(parent->name, id, new_field);
}

std::shared_ptr<FieldBase> VecField::clone() {
    auto new_field = std::make_shared<VecField>(base_type, m_components, m_memory_size, m_are_all_the_same_type);
    copy_data(new_field);
    new_field->optional_ordering = optional_ordering;
    new_field->optional_rows = optional_rows;
    new_field->optional_cols = optional_cols;
    return new_field;
}

std::shared_ptr<VecField> VecField::make_vec2_field(const EType pod_type) {
    ComponentVector components{{"x", pod_type}, {"y", pod_type}};

    return std::make_shared<VecField>(EType::VEC2, components, pod_type_size(pod_type) * components.size());
}

std::shared_ptr<VecField> VecField::make_vec3_field(const EType pod_type) {
    ComponentVector components{{"x", pod_type}, {"y", pod_type}, {"z", pod_type}};
    return std::make_shared<VecField>(EType::VEC3, components, pod_type_size(pod_type) * components.size());
}

std::shared_ptr<VecField> VecField::make_vec4_field(const EType pod_type) {
    ComponentVector components{{"x", pod_type}, {"y", pod_type}, {"z", pod_type}, {"w", pod_type}};
    return std::make_shared<VecField>(EType::VEC4, components, pod_type_size(pod_type) * components.size());
}

std::shared_ptr<VecField> VecField::make_euler_field(const EType pod_type, const EEulerOrdering ordering) {
    ComponentVector components{};

    switch (ordering) {
    case EEulerOrdering::XYZ:
        components = {{"x", pod_type}, {"y", pod_type}, {"z", pod_type}};
    case EEulerOrdering::XZY:
        components = {{"x", pod_type}, {"z", pod_type}, {"y", pod_type}};
    case EEulerOrdering::YXZ:
        components = {{"y", pod_type}, {"x", pod_type}, {"z", pod_type}};
    case EEulerOrdering::YZX:
        components = {{"y", pod_type}, {"z", pod_type}, {"x", pod_type}};
    case EEulerOrdering::ZXY:
        components = {{"z", pod_type}, {"x", pod_type}, {"y", pod_type}};
    case EEulerOrdering::ZYX:
        components = {{"z", pod_type}, {"y", pod_type}, {"x", pod_type}};
    default:
        break;
    }

    const auto new_field =
        std::make_shared<VecField>(EType::EULER, components, pod_type_size(pod_type) * components.size());
    new_field->optional_ordering = static_cast<size_t>(ordering);
    return new_field;
}

std::shared_ptr<VecField> VecField::make_color_field(const EType pod_type, const EColorType ordering) {
    ComponentVector components{};

    switch (ordering) {
    case EColorType::RGB:
        components = {{"r", pod_type}, {"g", pod_type}, {"b", pod_type}};
    case EColorType::RGBA:
        components = {{"r", pod_type}, {"g", pod_type}, {"b", pod_type}, {"a", pod_type}};
    case EColorType::ARGB:
        components = {{"a", pod_type}, {"r", pod_type}, {"g", pod_type}, {"b", pod_type}};
    case EColorType::BGRA:
        components = {{"b", pod_type}, {"g", pod_type}, {"r", pod_type}, {"a", pod_type}};
    case EColorType::ABGR:
        components = {{"a", pod_type}, {"b", pod_type}, {"g", pod_type}, {"r", pod_type}};
    default:
        break;
    }

    const auto new_field =
        std::make_shared<VecField>(EType::COLOR, components, pod_type_size(pod_type) * components.size());
    new_field->optional_ordering = static_cast<size_t>(ordering);
    return new_field;
}

std::shared_ptr<VecField> VecField::make_quaternion_field(const EType pod_type) {
    ComponentVector components{{"x", pod_type}, {"y", pod_type}, {"z", pod_type}, {"w", pod_type}};
    return std::make_shared<VecField>(EType::QUATERION, components, pod_type_size(pod_type) * components.size());
}

std::shared_ptr<VecField> VecField::make_matrix_field(const EType pod_type, const size_t rows, const size_t cols,
                                                      const bool is_row_major) {
    ComponentVector components{};
    components.reserve(rows * cols);

    const auto mem_size = pod_type_size(pod_type) * (rows * cols);

    if (is_row_major) {
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                components.push_back({fmt::format("m{}{}", i, j), pod_type});
            }
        }

        const auto new_field = std::make_shared<VecField>(EType::MATRIX_ROW_MAJOR, components, mem_size);
        new_field->optional_rows = rows;
        new_field->optional_cols = cols;
        return new_field;
    }

    for (size_t i = 0; i < cols; ++i) {
        for (size_t j = 0; j < rows; ++j) {
            components.push_back({fmt::format("m{}{}", i, j), pod_type});
        }
    }

    const auto new_field = std::make_shared<VecField>(EType::MATRIX_COLUMN_MAJOR, components, mem_size);
    new_field->optional_rows = rows;
    new_field->optional_cols = cols;
    return new_field;
}
