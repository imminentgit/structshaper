#include "PodField.h"

#include "../Struct.h"

#include <Gui/Gui.h>
#include <Gui/Views/MessagesView.h>

#include <misc/cpp/imgui_stdlib.h>

#include <ProcessInterface/ProcessInterface.h>

#include <OptionsManager/OptionsManager.h>

#include <Logger/Logger.h>

#include <Utils/ScopeGuard.h>
#include <Utils/expected_result.h>

EType default_pod_type_by_current_process() {
    const auto &current_process = ProcessInterface::the().current_process();
    return current_process.is_64_bit ? EType::I64 : EType::I32;
}

std::expected<void, std::string> PodField::write_field_value_from_string(const std::string_view str) {
    auto *process_interface = ProcessInterface::the().current_loaded_interface();
    if (!process_interface) {
        return std::unexpected("No process interface loaded");
    }

    const auto &current_process = ProcessInterface::the().current_process();
    if (!current_process.is_attached()) {
        return std::unexpected("No process attached");
    }

    const auto write_value = [&]<typename T>() -> std::expected<void, std::string> {
        T result{};
        const auto [_, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
        if (ec == std::errc()) {
            if (const auto res =
                    process_interface->write_process_memory(current_process.handle, address, &result, memory_size());
                !res) {
                return std::unexpected(res.error());
            }
        }
        else {
            return std::unexpected(std::make_error_code(ec).message());
        }

        return {};
    };

    if (base_type >= EType::I8 && base_type <= EType::U64) {
        return write_value.operator()<int64_t>();
    }

    if (base_type == EType::F32) {
        return write_value.operator()<float>();
    }

    if (base_type == EType::F64) {
        return write_value.operator()<double>();
    }

    return {};
}

std::expected<std::string, std::string> PodField::field_value_to_string() {
    const auto read_value = [&]<typename T> -> std::string {
        auto *process_interface = ProcessInterface::the().current_loaded_interface();
        const auto &current_process = ProcessInterface::the().current_process();

        if (is_pointer_to) {
            T result{};

            uintptr_t pointer_address{};
            if (const auto res = process_interface->read_process_memory(current_process.handle, address,
                                                                        &pointer_address, sizeof(uintptr_t));
                !res) {
                MessagesView::add(fmt::format("Failed to read memory for field {}: {}", name, res.error()), EMessageType::ERROR_MSG);
                return fmt::format("{}", T{});
            }

            if (const auto res =
                    process_interface->read_process_memory(current_process.handle, pointer_address, &result, sizeof(T));
                !res) {
                MessagesView::add(fmt::format("Failed to read memory for field {}: {}", name, res.error()), EMessageType::ERROR_MSG);
                return fmt::format("{}", T{});
            }

            return fmt::format("{}", result);
        }

        return fmt::format("{}", *reinterpret_cast<T *>(data_in_memory_buffer()));
    };

    switch (base_type) {
    case EType::I8:
        return read_value.operator()<int8_t>();
    case EType::I16:
        return read_value.operator()<int16_t>();
    case EType::I32:
        return read_value.operator()<int32_t>();
    case EType::I64:
        return read_value.operator()<int64_t>();
    case EType::U8:
        return read_value.operator()<uint8_t>();
    case EType::U16:
        return read_value.operator()<uint16_t>();
    case EType::U32:
        return read_value.operator()<uint32_t>();
    case EType::U64:
        return read_value.operator()<uint64_t>();
    case EType::F32:
        return read_value.operator()<float>();
    case EType::F64:
        return read_value.operator()<double>();
    default:
        return unexpected_fmt("Invalid pod type: {}", to_string(base_type));
    }
}

void PodField::render_pod_field(const bool is_named_mode) {
    using namespace ImGui::Helpers;

    render_field_name(is_named_mode);
    ScopeGuard guard(&ImGui::PopStyleVar, 1);

    if (!is_named_mode) {
        return;
    }

    const auto &options_mgr = OptionsManager::the();
    const auto &colors = options_mgr.options.colors;

    auto &x_offset = render_data.x_offset;

    const auto text_rect = ImGui::CalcTextSize(field_value_str);
    ImGui::SetNextItemWidth(text_rect.x + 1.f);

    ImGui::PushStyleColor(ImGuiCol_Text, colors[static_cast<size_t>(EColorIndex::FIELD_NAME_COLOR)]);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

    if (ImGui::InputText(make_imgui_id("_FieldValueInput"), &field_value_str,
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_NoHorizontalScroll)) {
        if (const auto res = write_field_value_from_string(field_value_str); !res) {
            Gui::the().add_notification({}, fmt::format("Failed to write value on field {}, with error: {}", name, res.error()));
        }
    }

    ImGui::PopStyleColor(2);

    if (!ImGui::IsItemFocused() && state == EFieldState::NAMED) {
        if (const auto res = field_value_to_string()) {
            field_value_str = res.value();
        }
        else {
            MessagesView::add(fmt::format("Failed to read value on field {}, with error: {}", name, res.error()), EMessageType::ERROR_MSG);
        }
    }

    x_offset += ImGui::GetItemRectSize().x;

    ImGui::SameLine(x_offset);
    colored_text_from_str(";", colors[static_cast<size_t>(EColorIndex::FIELD_NAME_COLOR)]);
    x_offset += get_char_width(';') + 1.f;
}

void PodField::render(const bool is_offscreen) {
    using namespace ImGui::Helpers;

    render_begin();
    if (state == EFieldState::NAMED || state == EFieldState::EDITING_NAME) {
        render_pod_field(state == EFieldState::NAMED);
    }

    render_end(is_offscreen);
}

size_t PodField::memory_size() {
    if (is_pointer_to) {
        return default_field_size_by_current_process();
    }

    return pod_type_size(base_type);
}

void PodField::reset_field() {
    FieldBase::reset_field();

    class_type = EFieldClassType::PodField;
}

void PodField::copy_data(std::shared_ptr<FieldBase> to_field) {
    FieldBase::copy_data(to_field);

    auto *to_pod_field = dynamic_cast<PodField *>(to_field.get());
    if (!to_pod_field) {
        return;
    }

    to_pod_field->base_type = base_type;
}

std::shared_ptr<FieldBase> PodField::clone() {
    auto field = std::make_shared<PodField>();
    copy_data(field);
    return field;
}

std::shared_ptr<PodField> PodField::make_default_pod_field() {
    return std::make_shared<PodField>(default_pod_type_by_current_process());
}