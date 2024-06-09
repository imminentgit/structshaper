#include "Struct.h"

#include <set>

#include "ProjectManager.h"

#include "Types/PodField.h"
#include "Types/StructField.h"
#include "Types/VecField.h"

#include <Gui/Gui.h>
#include <Gui/Views/MessagesView.h>

#include <ProcessInterface/ProcessInterface.h>

#include <Logger/Logger.h>

#include <Utils/expected_result.h>

#include <Queue/Event.h>

void Struct::read_memory(size_t new_mem_size) {
    auto *process_interface = ProcessInterface::the().current_loaded_interface();
    if (!process_interface) {
        MessagesView::add(fmt::format("Can't read memory on struct {}, due to no process interface being loaded", name),
                          EMessageType::WARNING_MSG);
        return;
    }

    const auto &current_process = ProcessInterface::the().current_process();
    if (!current_process.is_attached()) {
        MessagesView::add(fmt::format("Can't read memory on struct {}, due to no process being attached to", name),
                          EMessageType::WARNING_MSG);
        return;
    }

    const auto minimum_mem_size = default_field_size_by_current_process();
    if (new_mem_size < minimum_mem_size) {
        new_mem_size = minimum_mem_size;
    }

    auto &mem_buf = memory_buffer;
    if (mem_buf.size() != new_mem_size) {
        mem_buf.resize(new_mem_size);
    }

    if (address) {
        const auto res =
            process_interface->read_process_memory(current_process.handle, address, mem_buf.data(), mem_buf.size());
        if (!res) {
            if (res && *res < new_mem_size) {
                MessagesView::add(
                    fmt::format("Only read memory partially on struct {}, size required: {}, read size: {}", name, *res,
                                new_mem_size),
                    EMessageType::WARNING_MSG);
            }
            else {
                MessagesView::add(fmt::format("Failed to read memory on struct {}: {}", name, res.error()),
                                  EMessageType::ERROR_MSG);
            }
        }
    }
}

void Struct::rename_internal(const std::string &new_name) {
    name = new_name;
    obtain_struct_view()->title = new_name;
}

void Struct::update_field_offsets() {
    uintptr_t offset = 0;
    for (const auto &field : fields()) {
        field->offset = offset;
        offset += field->memory_size();
        field->address = address + field->offset;
    }
}

void Struct::update_struct_range() {
    bool found_begin = false;
    for (auto it = m_fields.begin(); it != m_fields.end(); ++it) {
        if ((*it)->state != EFieldState::UNNAMED) {
            begin = it;
            found_begin = true;
            break;
        }
    }

    if (!found_begin) {
        begin = m_fields.end();
    }

    auto found_end = false;
    for (auto it = m_fields.rbegin(); it != m_fields.rend(); ++it) {
        if ((*it)->state != EFieldState::UNNAMED) {
            end = it.base();
            found_end = true;
            break;
        }
    }

    if (!found_end) {
        end = m_fields.end();
    }
}

void Struct::update_everything() {
    update_field_offsets();
    update_struct_range();

    Event::OnStructUpdate::call(name);
}

Struct::FieldListType::iterator Struct::emplace_field(const FieldListType::iterator &pos,
                                                      const std::shared_ptr<FieldBase> &field,
                                                      const UniversalId custom_id) {
    const auto field_id = custom_id == IdAllocatorType::INVALID_ID ? m_field_id_allocator.allocate() : custom_id;
    field->init_base(this, field_id);

    auto it = m_fields.emplace(pos, field);
    m_field_name_to_iterator[field->name] = it;
    m_field_id_to_iterator[field_id] = it;

    LOG_INFO("Emplaced field with name: {}, id: {}", field->name, field_id);

    update_everything();

    auto &proj_manager = ProjectManager::the();
    proj_manager.mark_as_dirty();

    return std::move(it);
}

void Struct::emplace_fields(const FieldListType::iterator &pos, const size_t amount, const EType default_type) {
    for (size_t i = 0; i < amount; ++i) {
        const auto field_id = m_field_id_allocator.allocate();

        auto new_field = std::make_shared<DefaultFieldType>(default_type);
        new_field->init_base(this, field_id);
        new_field->set_default_name();

        const auto it = m_fields.emplace(pos, new_field);
        m_field_name_to_iterator[new_field->name] = it;
        m_field_id_to_iterator[field_id] = it;

        LOG_INFO("Emplaced field with name: {}, id: {}", new_field->name, field_id);
    }

    update_everything();

    auto &proj_manager = ProjectManager::the();
    proj_manager.mark_as_dirty();
}

std::expected<void, std::string> Struct::remove_field_by_pos(const FieldListType::iterator &pos) {
    const auto field_name = (*pos)->name;
    const auto field_id = (*pos)->id;

    m_fields.erase(pos);

    m_field_name_to_iterator.erase(field_name);
    m_field_id_to_iterator.erase(field_id);

    m_field_id_allocator.free(field_id);

    update_everything();

    auto &proj_manager = ProjectManager::the();
    proj_manager.mark_as_dirty();

    return {};
}

// TODO: Make an unexpected_fmt function that uses source location
std::expected<void, std::string> Struct::remove_field_by_id(const UniversalId id) {
    const auto it = m_field_id_to_iterator.find(id);
    if (it == m_field_id_to_iterator.end()) {
        return unexpected_fmt("remove_field_by_id: Failed to find field by id {}", id);
    }

    return remove_field_by_pos(it->second);
}

std::expected<void, std::string> Struct::remove_field_by_name(const std::string &field_name) {
    const auto it = m_field_name_to_iterator.find(field_name);
    if (it == m_field_name_to_iterator.end()) {
        return unexpected_fmt("remove_field_by_name: Failed to find field by name {}", field_name);
    }

    return remove_field_by_pos(it->second);
}

std::expected<void, std::string> Struct::rename_field_by_pos(const FieldListType::iterator &pos,
                                                             const std::string &new_name) {
    const auto it = m_field_name_to_iterator.find((*pos)->name);
    if (it != m_field_name_to_iterator.end()) {
        m_field_name_to_iterator.erase(it);
        m_field_name_to_iterator[new_name] = pos;
    }

    (*pos)->set_name(new_name);
    update_everything();

    auto &proj_manager = ProjectManager::the();
    proj_manager.mark_as_dirty();

    return {};
}

std::expected<void, std::string> Struct::rename_field_by_name(const std::string &field_name,
                                                              const std::string &new_name) {
    const auto it = m_field_name_to_iterator.find(field_name);
    if (it == m_field_name_to_iterator.end()) {
        return unexpected_fmt("rename_field_by_name: Failed to find field by name {}", field_name);
    }

    return rename_field_by_pos(it->second, new_name);
}

std::expected<void, std::string> Struct::rename_field_by_id(const UniversalId id, const std::string &new_name) {
    const auto it = m_field_id_to_iterator.find(id);
    if (it == m_field_id_to_iterator.end()) {
        return unexpected_fmt("rename_field_by_id: Failed to find field by id {}", id);
    }

    return rename_field_by_pos(it->second, new_name);
}

Struct::FieldListType::iterator Struct::obtain_field_by_name(const std::string &field_name) {
    const auto it = m_field_name_to_iterator.find(field_name);
    if (it == m_field_name_to_iterator.end()) {
        return m_fields.end();
    }

    return it->second;
}

Struct::FieldListType::iterator Struct::obtain_field_by_id(const UniversalId field_id) {
    const auto it = m_field_id_to_iterator.find(field_id);
    if (it == m_field_id_to_iterator.end()) {
        return m_fields.end();
    }

    return it->second;
}

void Struct::splice_field_to_pos(const FieldListType::iterator &from, const FieldListType::iterator &to) {
    std::swap(m_field_name_to_iterator[(*from)->name], m_field_name_to_iterator[(*to)->name]);
    std::swap(m_field_id_to_iterator[(*from)->id], m_field_id_to_iterator[(*to)->id]);

    std::iter_swap(from, to);

    update_everything();

    auto &proj_manager = ProjectManager::the();
    proj_manager.mark_as_dirty();
}

void Struct::clear_fields() {
    m_fields.clear();
    m_field_name_to_iterator.clear();
    m_field_id_to_iterator.clear();
    m_field_id_allocator.clear();

    begin = m_fields.end();
    end = m_fields.end();
}

std::shared_ptr<StructView> Struct::obtain_struct_view() const {
    const auto &views = Gui::the().views;
    return std::static_pointer_cast<StructView>(views.at(ui_view_id));
}

void Struct::deregister_struct_view() const { BaseView::remove_view(ui_view_id); }

void Struct::serialize(nlohmann::json &json) {
    const auto struct_window = obtain_struct_view();
    struct_window->serialize(json);

    json["address"] = address;
    json["consume_fields"] = consume_fields;
    json["show_rtti_type_info"] = show_rtti_type_info;

    m_field_id_allocator.serialize(json);

    struct CombinedField {
        FieldListType::iterator begin{};
        size_t count{}; // The number of fields excluding the first one

        ALWAYS_INLINE bool is_packed() const { return count > 0; }
    };

    std::vector<CombinedField> combined_fields{};
    auto current = m_fields.end();
    size_t count{};

    for (auto it = m_fields.begin(); it != m_fields.end(); ++it) {
        auto &field = *it;
        if (field->state == EFieldState::NAMED) {
            if (current != m_fields.end()) {
                combined_fields.push_back({current, count});
                current = m_fields.end();
                count = 0;
            }

            combined_fields.push_back({it, 0});
            continue;
        }

        if (current != m_fields.end()) {
            if ((*current)->type_name() != field->type_name()) {
                combined_fields.push_back({current, count});
                current = m_fields.end();
                count = 0;
            }
        }

        if (current == m_fields.end()) {
            current = it;
        }
        else {
            ++count;
        }
    }

    if (current != m_fields.end()) {
        combined_fields.push_back({current, count});
    }

    auto &json_fields = json["fields"];
    for (const auto &combined : combined_fields) {
        if (combined.is_packed()) {
            nlohmann::json packed_field{};
            packed_field["field_type"] = (*combined.begin)->class_type;
            packed_field["count"] = combined.count + 1;

            switch ((*combined.begin)->class_type) {
            case EFieldClassType::PodField: {
                const auto pod_field = std::static_pointer_cast<PodField>(*combined.begin);
                packed_field["base_type"] = pod_field->base_type;
                break;
            }
            default:
                break;
            }

            json_fields.emplace_back(packed_field);
            continue;
        }

        nlohmann::json named_field{};
        (*combined.begin)->serialize(named_field);
        named_field["count"] = 1;
        json_fields.emplace_back(named_field);
    }
}

void Struct::deserialize(const nlohmann::json &json) {
    address = json.value("address", 0);
    consume_fields = json.value("consume_fields", false);
    show_rtti_type_info = json.value("show_rtti_type_info", false);

    m_field_id_allocator.deserialize(json);

    auto &json_fields = json.at("fields");

    UniversalId id_counter = 1;

    std::set<UniversalId> field_ids{};
    for (const auto &field : json_fields) {
        const auto id = field.value("id", 0);
        auto has_state_field = field.find("state") != field.end();

        if (!has_state_field) {
            LOG_WARN("Field with id {} does not have a state field", id);
            continue;
        }

        auto named = field.at("state").get<EFieldState>() == EFieldState::NAMED;

        if (!named) {
            continue;
        }

        field_ids.emplace(id);
    }

    const auto emplace_single_field = [this](const nlohmann::json &field_json,
                                             const std::shared_ptr<FieldBase> &new_field, UniversalId id) {
        new_field->deserialize(field_json);
        emplace_back_field(new_field, id);

        // Make sure the id allocator gets updated
        m_field_id_allocator.remove_free_id(id);
    };

    for (const auto &field : json_fields) {
        const auto field_type = field.at("field_type").get<EFieldClassType>();
        const auto count = field.at("count").get<size_t>();
        const auto id = field.value("id", 0);

        switch (field_type) {
        case EFieldClassType::PodField: {
            // TODO: Maybe use an uuid system instead in the future
            auto named = field.value("state", EFieldState::UNNAMED) == EFieldState::NAMED;
            if (named) {
                emplace_single_field(field, std::make_shared<PodField>(), id);
            }
            else {
                const auto base_type = field.at("base_type").get<EType>();
                for (size_t i = 0; i < count; ++i) {
                    while (field_ids.contains(id_counter)) {
                        ++id_counter;
                    }

                    auto new_field = std::make_shared<PodField>(base_type);
                    emplace_back_field(new_field, id_counter);
                    new_field->set_default_name();

                    // Make sure the id allocator gets updated
                    m_field_id_allocator.remove_free_id(id_counter);

                    ++id_counter;
                }
            }
            break;
        }
        case EFieldClassType::VecField: {
            emplace_single_field(field, std::make_shared<VecField>(), id);
            break;
        }
        case EFieldClassType::StructField: {
            emplace_single_field(field, std::make_shared<StructField>(), id);
            break;
        }
        default:
            break;
        }
    }

    update_everything();
}
