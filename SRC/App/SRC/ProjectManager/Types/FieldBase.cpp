#include "FieldBase.h"
#include "../Struct.h"

#include <fmt/format.h>
#include <misc/cpp/imgui_stdlib.h>
#include <Gui/IconsLoader/IconsLoader.h>

#include <ProcessInterface/ProcessInterface.h>
#include <OptionsManager/OptionsManager.h>

#include <Queue/Event.h>
#include <Utils/ScopeGuard.h>

#include <Logger/Logger.h>

#include "StructField.h"
#include "VecField.h"

size_t default_field_size_by_current_process() {
    const auto &current_process = ProcessInterface::the().current_process();
    return current_process.is_64_bit ? 8 : 4;
}

void FieldBase::init_base(Struct *parent_struct, const UniversalId _id) {
    parent = parent_struct;
    id = _id;

    render_data.resize_buffers(memory_size());
}

void FieldBase::update_render_bounds() {
    // Look away, or im behind you
    ImGui::Helpers::ScopedCursorPos pos({-99999.f, -99999.f});

    render(true);
    render_bounds = ImGui::GetItemRectSize();
}

bool FieldBase::is_clipping_needed() {
    const auto current_draw_pos = ImGui::GetCursorScreenPos();
    return current_draw_pos.y > parent->struct_render_pos.y + parent->struct_render_bounds.y ||
        current_draw_pos.y + render_bounds.y < parent->struct_render_pos.y;
}

void FieldBase::render_field_info() {
    using namespace ImGui::Helpers;

    const auto &style = ImGui::GetStyle();
    const auto default_char_size = get_char_width('?');
    const auto spacing = style.ItemSpacing;

    const auto &options_mgr = OptionsManager::the();
    const auto &colors = options_mgr.options.colors;
    auto &x_offset = render_data.x_offset;

    std::array<char, 128 + 1> buffer{};
    auto fmt = fmt::format_to_n(buffer.begin(), buffer.size(), "0x{:04X}", offset);
    colored_text_from_str({buffer.data(), fmt.size}, colors[static_cast<size_t>(EColorIndex::OFFSET_COLOR)]);

    x_offset += default_char_size * (offset >= 0x10000 ? 7.f : 6.f) + 1.f + spacing.x;
    ImGui::SameLine(x_offset);

    if (ImGui::BeginItemTooltip()) {
        buffer.fill('\0');
        fmt = fmt::format_to_n(buffer.begin(), buffer.size(), "{}", offset);
        ImGui::TextUnformatted({buffer.data(), fmt.size});
        ImGui::EndTooltip();
    }

    // TODO: Limit the type name to 512 chars max, or make it use a dynamic buffer
    if (state != EFieldState::NAMED && state != EFieldState::EDITING_NAME) {
        buffer.fill('\0');
        fmt = fmt::format_to_n(buffer.begin(), buffer.size(), "{}", type_name());
        colored_text_from_str({buffer.data(), fmt.size}, colors[static_cast<size_t>(EColorIndex::UNNAMED_TYPE_COLOR)]);
        x_offset += default_char_size * static_cast<float>(fmt.size) + 1.f + spacing.x;
        ImGui::SameLine(x_offset);

        if (ImGui::BeginItemTooltip()) {
            buffer.fill('\0');
            fmt = fmt::format_to_n(buffer.begin(), buffer.size(), "{}", memory_size());
            ImGui::TextUnformatted({buffer.data(), fmt.size});
            ImGui::EndTooltip();
        }
    }

    buffer.fill('\0');
    fmt = fmt::format_to_n(buffer.begin(), buffer.size(), "0x{:08X}", address);
    colored_text_from_str({buffer.data(), fmt.size}, colors[static_cast<size_t>(EColorIndex::ADDRESS_COLOR)]);
    x_offset += default_char_size * static_cast<float>(fmt.size) + 1.f + spacing.x;
    ImGui::SameLine(x_offset);
}

void FieldBase::render_field_hex_info(const bool draw_first_bytes) {
    using namespace ImGui::Helpers;

    // The render buffers get resized in render_begin
    auto num_bytes = memory_size();

    const auto default_char_size = get_char_width('?');

    auto &style = ImGui::GetStyle();

    const auto &process_iface = ProcessInterface::the();

    const auto &options_mgr = OptionsManager::the();
    const auto &colors = options_mgr.options.colors;

    auto &x_offset = render_data.x_offset;

    auto show_range = render_data.hex_view_is_collapsed ? num_bytes / 2 : num_bytes;
    if (num_bytes == 3) {
        show_range = 3;
    }

    if (draw_first_bytes) {
        std::ranges::fill(render_data.first_n_bytes, '.');
        for (size_t i = 0; i < show_range; ++i) {
            const auto read_char = static_cast<char>(data_in_memory_buffer()[i]);
            auto &c = render_data.first_n_bytes[i];

            c = read_char > 0x20 && read_char < 0x7E ? read_char : '.';

            std::array char_str{c, '\0'};
            const auto char_size = get_char_width(c);

            ImGui::SameLine(x_offset);
            colored_text_from_str({char_str.data(), 1}, colors[static_cast<size_t>(EColorIndex::ASCII_VIEW_COLOR)]);
            x_offset += char_size + 1.f;
        }

        x_offset += style.ItemSpacing.x - style.FramePadding.x;
    }

    for (size_t i = 0; i < show_range; ++i) {
        std::array<char, 2> hex{};
        fmt::format_to_n(hex.begin(), hex.size(), "{:02X}", data_in_memory_buffer()[i]);

        render_data.edit_data[i * 3 + 0] = hex[0];
        render_data.edit_data[i * 3 + 1] = hex[1];
        render_data.edit_data[i * 3 + 2] = '\0';
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {style.FramePadding.x, 0.f});
    ImGui::PushStyleColor(ImGuiCol_Text, colors[static_cast<size_t>(EColorIndex::HEX_VIEW_COLOR)]);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

    size_t char_offset = 0;
    for (size_t i = 0; i < show_range; ++i) {
        std::array<char, 256 + 1> name_buf{};
        fmt::format_to_n(name_buf.begin(), name_buf.size(), "##FieldHexInput_{}_{}_{}_byte_{}", x_offset, address, offset, i);

        ImGui::SameLine(x_offset);
        ImGui::SetNextItemWidth(default_char_size * 4.f);

        auto *byte = &render_data.edit_data[char_offset];
        if (ImGui::InputText(name_buf.data(), byte, 2 + 1, ImGuiInputTextFlags_EnterReturnsTrue)) {
            auto new_byte = static_cast<uint8_t>(std::strtoul(byte, nullptr, 16));

            if (const auto res = process_iface.current_loaded_interface()->write_process_memory(
                    ProcessInterface::the().current_process().handle, address + i, &new_byte, sizeof(new_byte));
                !res) {
                Gui::the().add_notification(fmt::format("Failed to write memory on address 0x{:X}:", address),
                                            res.error());
            }
        }

        char_offset += 3;
        x_offset += ImGui::GetItemRectSize().x;
    }

    ImGui::SameLine(x_offset);

    const auto button_size = ImVec2(20.f, ImGui::GetTextLineHeight());

    const auto icon = render_data.hex_view_is_collapsed ? "arrow_expand"_svg_icon(button_size)
                                                        : "arrow_collapse"_svg_icon(button_size);
    push_dark_style_button();
    if (title_bar_button(make_imgui_id("_ExpandCollapseButton"), icon, button_size)) {
        render_data.hex_view_is_collapsed = !render_data.hex_view_is_collapsed;
    }
    pop_dark_style_button();

    if (ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted("Expand/Collapse hex view");
        ImGui::EndTooltip();
    }

    x_offset += button_size.x + style.ItemSpacing.x;

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::SameLine(x_offset);
}

void FieldBase::render_field_float_info() {
    using namespace ImGui::Helpers;

    auto& x_offset = render_data.x_offset;

    const auto &options_mgr = OptionsManager::the();
    const auto &colors = options_mgr.options.colors;

    colored_text_from_str("//", colors[static_cast<size_t>(EColorIndex::SEPARATOR_COLOR)]);
    x_offset += get_char_width('/') * 2.f + 10.f;
    ImGui::SameLine(x_offset);

    std::array<char, 128 + 1> buffer{};
    const auto [_, size] = fmt::format_to_n(buffer.begin(), buffer.size(), "({:.3f}, {})",
                                            *reinterpret_cast<float *>(data_in_memory_buffer()),
                                            *reinterpret_cast<int *>(data_in_memory_buffer()));

    const auto buffer_view = std::string_view{buffer.data(), size};
    colored_text_from_str(buffer_view, colors[static_cast<size_t>(EColorIndex::FLOAT_VALUE_COLOR)]);
    x_offset += ImGui::CalcTextSize(buffer_view).x + 1.f;
}

void FieldBase::render_field_name(const bool is_named_mode, const bool do_equal_sign,
                                  const std::function<void()> &after_type_name) {
    using namespace ImGui::Helpers;

    const auto &style = ImGui::GetStyle();

    const auto &options_mgr = OptionsManager::the();
    const auto &colors = options_mgr.options.colors;

    auto &x_offset = render_data.x_offset;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.f, 0.f});

    const auto type_name_str = type_name();
    colored_text_from_str(type_name_str, colors[static_cast<size_t>(EColorIndex::NAMED_TYPE_COLOR)]);

    x_offset += ImGui::CalcTextSize(type_name_str).x + style.ItemSpacing.x;

    ImGui::SameLine(x_offset);

    if (after_type_name) {
        after_type_name();
    }

    ImGui::SameLine(x_offset);

    if (!is_named_mode) {
        ImGui::SetKeyboardFocusHere(0);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, colors[static_cast<size_t>(EColorIndex::FIELD_NAME_COLOR)]);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

    auto filter = [](ImGuiInputTextCallbackData *data) {
        if (data->Buf && data->EventFlag & ImGuiInputTextFlags_CallbackAlways) {
            if (data->BufTextLen > 64) {
                const auto delta = data->BufTextLen - 64;
                data->DeleteChars(63, delta);
                return 1;
            }

            if (std::isdigit(data->Buf[0])) {
                data->DeleteChars(0, 1);
                return 1;
            }
        }

        if (data->EventFlag & ImGuiInputTextFlags_CallbackCharFilter) {
            const auto c = data->EventChar;
            return (!std::isalnum(c) && c != '_') || c == ' ' ? 1 : 0;
        }

        return 0;
    };

    auto text_rect = ImGui::CalcTextSize(render_data.tmp_name);
    text_rect.x += 1.f;
    render_data.tmp_name.empty() ? ImGui::SetNextItemWidth(20.f) : ImGui::SetNextItemWidth(text_rect.x);

    if (ImGui::InputText(make_imgui_id("_FieldNameInput"), &render_data.tmp_name,
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_NoHorizontalScroll |
                             ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter,
                         filter)) {

        if (const auto res = parent->rename_field_by_id(id, render_data.tmp_name); res) {
            if (!is_named_mode) {
                state = EFieldState::NAMED;
            }
        }
        else {
            ImGui::SetKeyboardFocusHere(0);
            Gui::the().add_notification(fmt::format("Failed to rename field: {}", render_data.tmp_name), res.error());
            render_data.tmp_name.clear();
        }
    }

    x_offset += ImGui::GetItemRectSize().x;

    ImGui::PopStyleColor(2);

    if (!is_named_mode && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        state = EFieldState::UNNAMED;
    }

    if (!is_named_mode) {
        ImGui::SameLine(x_offset);
        ImGui::TextUnformatted(";");
        return;
    }

    if (do_equal_sign) {
        x_offset += style.ItemSpacing.x;

        ImGui::SameLine(x_offset);
        ImGui::TextUnformatted("=");

        x_offset += get_char_width('=') + style.ItemSpacing.x;
        ImGui::SameLine(x_offset);
    }
}

constexpr auto DRAG_DROP_KEY = "FieldDragDrop";
void FieldBase::render_begin(const bool skip_inner_draw_calls) {
    using namespace ImGui::Helpers;

    render_data.resize_buffers(memory_size());

#ifdef STRUCTSHAPER_DEV
    if (parent->show_debug_bb) {
        const auto cursor = ImGui::GetCursorScreenPos();
        ImGui::GetForegroundDrawList()->AddRect(cursor, cursor + render_bounds, IM_COL32(255, 0, 0, 255));
    }
#endif

    if (is_dummy) {
        render_data.x_offset = 0;
    }

    ImGui::BeginGroup();

    if (!is_dummy) {
        const auto unique_id = make_imgui_id("_DragDrop");
        ImGui::PushID(unique_id);

        const auto button_size = ImVec2(20.f, ImGui::GetTextLineHeight());
        three_dot_button(unique_id, button_size);

        const auto &style = ImGui::GetStyle();
        render_data.x_begin = render_data.x_offset = button_size.x + style.ItemSpacing.x;

        if (ImGui::BeginDragDropSource()) {
            // Set payload to carry the index of our item (could be anything)
            ImGui::SetDragDropPayload(DRAG_DROP_KEY, &id, sizeof(UniversalId));

            ImGui::TextUnformatted(name);

            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted(name);
            ImGui::EndTooltip();
        }

        ImGui::PopID();

        ImGui::SameLine(render_data.x_offset);
    }

    if (!skip_inner_draw_calls) {
        render_field_info();

        if (state == EFieldState::UNNAMED) {
            render_field_hex_info();
            render_field_float_info();
        }
        else if (state == EFieldState::NAMED && !parent->hide_hex_views_on_named_fields) {
            render_field_hex_info(false);
        }
    }
}

void FieldBase::render_context_menu_contents() {
    if (!is_dummy) {
        const auto &field_pod_types = OptionsManager::the().options.field_pod_types;

        render_set_type_menu();

        // TODO: Change this with the new system
        if (ImGui::BeginMenu("Insert above")) {
            ScopeGuard insert_above_guard(&ImGui::EndMenu);
            for (size_t i = 0; i < static_cast<size_t>(EType::COUNT_POD_TYPES); ++i) {
                if (ImGui::MenuItem(field_pod_types[i])) {
                    const auto current_it = parent->obtain_field_by_id(id);
                    parent->emplace_fields(current_it, 1, static_cast<EType>(i));
                }
            }
        }

        if (ImGui::BeginMenu("Insert below")) {
            ScopeGuard insert_below_guard(&ImGui::EndMenu);
            for (size_t i = 0; i < static_cast<size_t>(EType::COUNT_POD_TYPES); ++i) {
                if (ImGui::MenuItem(field_pod_types[i])) {
                    const auto current_it = parent->obtain_field_by_id(id);
                    parent->emplace_fields(std::next(current_it), 1, static_cast<EType>(i));
                }
            }
        }

        if (state == EFieldState::NAMED && ImGui::MenuItem("Reset")) {
            reset_field();
            parent->update_everything();

            Gui::the().add_notification("Reset field", name);
        }

        if (ImGui::MenuItem("Remove")) {
            Event::RemoveField::call(parent->name, id);
        }
    }

    if (ImGui::BeginMenu("Copy")) {
        ScopeGuard copy_guard(&ImGui::EndMenu);
        std::array<char, 32> buffer{};
        if (ImGui::MenuItem("Address")) {
            fmt::format_to_n(buffer.begin(), buffer.size(), "0x{:X}", address);
            ImGui::SetClipboardText(buffer.data());
            Gui::the().add_notification("Copied address to clipboard", buffer.data());
        }

        if (ImGui::MenuItem("Offset")) {
            fmt::format_to_n(buffer.begin(), buffer.size(), "0x{:X}", offset);
            ImGui::SetClipboardText(buffer.data());
            Gui::the().add_notification("Copied offset to clipboard", buffer.data());
        }

        // TODO: Implement copy value
    }

    if (!is_dummy) {
        ImGui::Checkbox("Show all values", &show_all_values);
    }
}

void FieldBase::render_end(const bool is_offscreen) {
    using namespace ImGui::Helpers;

    auto& x_offset = render_data.x_offset;

    auto &proc_iface = ProcessInterface::the();
    auto &cur_proc = proc_iface.current_process();

    std::vector<uintptr_t> indirections{};
    cur_proc.get_indirections_for_address(address, indirections);

    if (!indirections.empty())
    {
        // We essentially only want to show the first indirection
        const auto first_indirection = indirections.front();
        auto* const module = cur_proc.get_module_from_address(first_indirection);

        ImGui::SameLine(x_offset);
        const auto arrow_str = " -> ";
        colored_text_from_str(arrow_str, IM_COL32(255, 0, 0, 255));
        x_offset += ImGui::CalcTextSize(arrow_str).x + 1.f;

        if (module) {
            ImGui::SameLine(x_offset);
            colored_text_from_str(module->name, IM_COL32(255, 0, 0, 255));
            x_offset += ImGui::CalcTextSize(module->name).x + 1.f;
        }

        ImGui::SameLine(x_offset);

        const auto addr_str = fmt::vformat(module ? " 0x{:X}" :  "0x{:X}", fmt::make_format_args(first_indirection));
        ImGui::SameLine(x_offset);
        colored_text_from_str(addr_str, IM_COL32(255, 0, 0, 255));

        if (ImGui::IsItemClicked()) {
            ImGui::SetClipboardText(addr_str);
            Gui::the().add_notification("Copied address to clipboard", module ? addr_str.substr(1) : addr_str);
        }

        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted("Click to copy address");
            ImGui::EndTooltip();
        }

        x_offset += ImGui::CalcTextSize(addr_str).x + 1.f;

        if (module) {
            const auto rtti_name_res = cur_proc.get_rtti_name(first_indirection, module);
            if (rtti_name_res) {
                const auto rtti_str = fmt::format(": {}", *rtti_name_res);
                ImGui::SameLine(x_offset);
                colored_text_from_str(rtti_str, IM_COL32(255, 0, 0, 255));
                x_offset += ImGui::CalcTextSize(rtti_str).x + 1.f;
            }
        }
    }

    if (show_all_values) {
        ImGui::Helpers::ScopedCursorPos cursor_pos({render_data.x_begin, ImGui::GetCursorPosY()});
        const auto *pod_fields = reinterpret_cast<const StructPodFieldUnion *>(data_in_memory_buffer());

        std::array<char, 512 + 1> tooltip_str{};
        const auto [_, size] = fmt::format_to_n(
            tooltip_str.begin(), tooltip_str.size(),
            "I8: {0}, 0x{0:X}, I16: {1}, 0x{1:X}, I32: {2}, 0x{2:X}, I64: {3}, 0x{3:X}, F32: {4}\nU8: {5}, "
            "0x{5:X}, U16: {6}, 0x{6:X}, U32: {7}, 0x{7:X}, U64: {8}, 0x{8:X}, F64: {9}",
            pod_fields->i8_val, pod_fields->i16_val, pod_fields->i32_val, pod_fields->i64_val, pod_fields->f32_val,
            pod_fields->u8_val, pod_fields->u16_val, pod_fields->u32_val, pod_fields->u64_val, pod_fields->f64_val);

        ImGui::TextUnformatted({tooltip_str.data(), size});
    }

    ImGui::EndGroup();

    if (!is_offscreen) {
        grab_render_bounds();
    }

    if (!is_dummy && ImGui::BeginDragDropTarget()) {
        if (const auto *payload = ImGui::AcceptDragDropPayload(DRAG_DROP_KEY)) {
            const auto from_id = *static_cast<UniversalId *>(payload->Data);
            Event::MoveStruct::call(parent->name, from_id, id);
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem(make_imgui_id("_ContextMenu"))) {
        render_context_menu_contents();
        ImGui::EndPopup();
    }
}

void FieldBase::render_set_type_menu() {
    if (!ImGui::BeginMenu("Set type")) {
        return;
    }

    ScopeGuard set_type_guard(&ImGui::EndMenu);

    const auto &options_mgr = OptionsManager::the();
    const auto &field_types = options_mgr.options.field_pod_types;

    if (ImGui::BeginMenu("Pod")) {
        ScopeGuard pod_guard(&ImGui::EndMenu);
        for (size_t i = 0; i < static_cast<size_t>(EType::COUNT_POD_TYPES); ++i) {
            const auto type = static_cast<EType>(i);
            if (ImGui::MenuItem(field_types[i])) {
                // TODO: Get rid of this after consumption is implemented, or maybe move it to the event
                /*if (class_type == EFieldClassType::PodField) {
                    base_type = type;
                    state = EFieldState::EDITING_NAME;

                    parent->update_everything();
                }
                else {*/
                auto new_field = std::make_shared<PodField>();
                copy_data(new_field);

                new_field->base_type = type;
                new_field->state = EFieldState::EDITING_NAME;

                Event::SwapField::call(parent->name, id, new_field);
                //}
            }
        }
    }

    if (ImGui::BeginMenu("Vec")) {
        ScopeGuard vec_guard(&ImGui::EndMenu);

        for (auto i = static_cast<size_t>(EType::VEC2); i < static_cast<size_t>(EType::VEC4) + 1; ++i) {
            const auto vec_type = static_cast<EType>(i);

            if (ImGui::BeginMenu(field_types[i])) {
                ScopeGuard vec_type_guard(&ImGui::EndMenu);
                for (size_t j = 0; j < static_cast<size_t>(EType::COUNT_POD_TYPES); ++j) {
                    const auto pod_type = static_cast<EType>(j);
                    if (ImGui::MenuItem(field_types[j])) {
                        std::shared_ptr<VecField> new_field{};
                        switch (vec_type) {
                        case EType::VEC2:
                            new_field = VecField::make_vec2_field(pod_type);
                            break;
                        case EType::VEC3:
                            new_field = VecField::make_vec3_field(pod_type);
                            break;
                        case EType::VEC4:
                            new_field = VecField::make_vec4_field(pod_type);
                            break;
                        default:
                            break;
                        }

                        copy_data(new_field);
                        new_field->state = EFieldState::EDITING_NAME;
                        Event::SwapField::call(parent->name, id, new_field);
                    }
                }
            }
        }
    }

    if (ImGui::BeginMenu("Euler")) {
        ScopeGuard euler_menu_guard(&ImGui::EndMenu);

        for (auto i = 0; i < static_cast<size_t>(EEulerOrdering::MAX); ++i) {
            const auto euler_ordering = static_cast<EEulerOrdering>(i);

            if (ImGui::BeginMenu(to_string(euler_ordering))) {
                ScopeGuard euler_ordering_guard(&ImGui::EndMenu);
                for (size_t j = 0; j < static_cast<size_t>(EType::COUNT_POD_TYPES); ++j) {
                    const auto pod_type = static_cast<EType>(j);
                    if (ImGui::MenuItem(field_types[j])) {
                        std::shared_ptr<VecField> new_field = VecField::make_euler_field(pod_type, euler_ordering);
                        copy_data(new_field);
                        new_field->state = EFieldState::EDITING_NAME;
                        Event::SwapField::call(parent->name, id, new_field);
                    }
                }
            }
        }
    }

    if (ImGui::BeginMenu("Color")) {
        ScopeGuard color_menu_guard(&ImGui::EndMenu);

        for (auto i = 0; i < static_cast<size_t>(EColorType::MAX); ++i) {
            const auto color_type = static_cast<EColorType>(i);

            if (ImGui::BeginMenu(to_string(color_type))) {
                ScopeGuard color_type_guard(&ImGui::EndMenu);
                for (size_t j = 0; j < static_cast<size_t>(EType::COUNT_POD_TYPES); ++j) {
                    const auto pod_type = static_cast<EType>(j);
                    if (ImGui::MenuItem(field_types[j])) {
                        std::shared_ptr<VecField> new_field = VecField::make_color_field(pod_type, color_type);
                        copy_data(new_field);
                        new_field->state = EFieldState::EDITING_NAME;
                        Event::SwapField::call(parent->name, id, new_field);
                    }
                }
            }
        }
    }

    if (ImGui::BeginMenu("Quaternion")) {
        ScopeGuard quat_menu_guard(&ImGui::EndMenu);

        for (size_t i = 0; i < static_cast<size_t>(EType::COUNT_POD_TYPES); ++i) {
            if (ImGui::MenuItem(field_types[i])) {
                std::shared_ptr<VecField> new_field = VecField::make_quaternion_field(static_cast<EType>(i));
                copy_data(new_field);
                new_field->state = EFieldState::EDITING_NAME;
                Event::SwapField::call(parent->name, id, new_field);
            }
        }
    }

    if (ImGui::BeginMenu("Matrix")) {
        ScopeGuard matrix_menu_guard(&ImGui::EndMenu);

        auto &rows = parent->matrix_rows;
        auto &cols = parent->matrix_cols;

        ImGui::InputInt("Rows", &rows);
        ImGui::InputInt("Cols", &cols);

        std::array row_orderings = {"Row major", "Column major"};
        for (const auto [idx, row_order] : row_orderings | std::ranges::views::enumerate) {
            if (ImGui::BeginMenu(row_order)) {
                ScopeGuard row_menu_guard(&ImGui::EndMenu);

                for (size_t i = 0; i < static_cast<size_t>(EType::COUNT_POD_TYPES); ++i) {
                    if (ImGui::MenuItem(field_types[i])) {
                        std::shared_ptr<VecField> new_field =
                            VecField::make_matrix_field(static_cast<EType>(i), rows, cols, idx == 0);
                        copy_data(new_field);
                        new_field->state = EFieldState::EDITING_NAME;
                        Event::SwapField::call(parent->name, id, new_field);
                    }
                }
            }
        }
    }

    auto &proj_manager = ProjectManager::the();
    auto &structs = proj_manager.structs;

    if (structs.size() > 1 && ImGui::BeginMenu("Struct")) {
        ScopeGuard struct_menu_guard(&ImGui::EndMenu);

        for (const auto &[other_struct_name, other_struct_data] : structs) {
            if (other_struct_name == parent->name) {
                continue;
            }

            if (ImGui::MenuItem(other_struct_name)) {
                auto new_field = std::make_shared<StructField>(other_struct_name);
                copy_data(new_field);
                new_field->state = EFieldState::EDITING_NAME;
                Event::SwapField::call(parent->name, id, new_field);
            }
        }
    }
}

std::string FieldBase::type_name() {
    const auto &field_types = OptionsManager::the().options.field_pod_types;
    if (is_pointer_to) {
        return fmt::format("*{}*", field_types[static_cast<size_t>(base_type)]);
    }

    return field_types[static_cast<size_t>(base_type)];
}

void FieldBase::serialize(nlohmann::json &json) {
    json["name"] = name;
    json["id"] = id;
    json["offset"] = offset;
    json["state"] = state;
    json["field_type"] = class_type;
    json["base_type"] = base_type;
}

void FieldBase::deserialize(const nlohmann::json &json) {
    id = json.at("id").get<UniversalId>();
    offset = json.at("offset").get<ptrdiff_t>();
    state = json.at("state").get<EFieldState>();
    set_name(json.at("name").get<std::string>());
    class_type = json.at("field_type").get<EFieldClassType>();
    base_type = json.value("base_type", default_pod_type_by_current_process());
}

uint8_t *FieldBase::data_in_memory_buffer() const {
    auto &proc_iface = ProcessInterface::the();
    if (proc_iface.current_process().is_attached()) [[likely]] {
        return parent->memory_buffer.data() + offset;
    }

    return parent->memory_buffer.data();
}

void FieldBase::reset_field() {
    state = EFieldState::UNNAMED;
    is_pointer_to = false;

    if (const auto res = parent->rename_field_by_id(id, fmt::format("unnamed_{}", id)); !res) {
        Gui::the().add_notification(fmt::format("Failed to reset field: {}", name), res.error());
    }
}

void FieldBase::copy_data(const std::shared_ptr<FieldBase> to_field) {
    to_field->init_base(parent, id);
    to_field->set_name(name);

    to_field->id = id;
    to_field->offset = offset;
    to_field->address = address;

    to_field->state = state;

    // to_field->field_type = field_type;

    to_field->show_all_values = show_all_values;
    to_field->is_dummy = is_dummy;
}
