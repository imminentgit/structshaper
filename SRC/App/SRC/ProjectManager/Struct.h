#pragma once
#include "Types/FieldBase.h"

#include <expected>
#include <list>
#include <string>
#include <utility>

#include <Gui/Views/StructView.h>
#include <nlohmann/json.hpp>

#include <Utils/IDAllocator.h>

#include <Defines.h>

class Struct {
public:
    using FieldListType = std::list<std::shared_ptr<FieldBase>>;

    using IdAllocatorType = IdAllocator<>;

private:
    IdAllocatorType m_field_id_allocator{};

    FieldListType m_fields{};

    // For fast lookup e.g. when deleting a field, or if a field exists and such
    std::unordered_map<std::string, FieldListType::iterator> m_field_name_to_iterator{};

    std::unordered_map<UniversalId, FieldListType::iterator> m_field_id_to_iterator{};

public:
    std::string name{};

    uintptr_t address{};

    std::vector<uint8_t> memory_buffer{};

    // The actual struct window is stored in the ui side of things
    size_t ui_view_id{};

    ImVec2 struct_render_pos{};

    ImVec2 struct_render_bounds{};

    bool is_in_edit_mode{};

    std::string tmp_new_name{};

    FieldListType::iterator begin{};

    FieldListType::iterator end{};

#ifdef STRUCTSHAPER_DEV
    bool show_debug_bb = false;
#endif

    bool consume_fields = true;

    bool hide_hex_views_on_named_fields = true;

    bool show_rtti_type_info = true;

    int matrix_rows = 4;

    int matrix_cols = 4;

    Struct() { memory_buffer.resize(8); }

    Struct(std::string name) : name(std::move(name)) { memory_buffer.resize(8); }

    void read_memory(size_t new_mem_size);

    // Do not use this function directly, use ProjectManager::rename_struct instead
    void rename_internal(const std::string &new_name);

    void update_field_offsets();

    void update_struct_range();

    void update_everything();

    // WARNING: Those two do not set the default name, please set it manually or use field->set_default_name()
    FieldListType::iterator emplace_field(const FieldListType::iterator &pos, const std::shared_ptr<FieldBase> &field,
                                          UniversalId custom_id = IdAllocatorType::INVALID_ID);

    ALWAYS_INLINE FieldListType::iterator
    emplace_back_field(const std::shared_ptr<FieldBase> &field,
                       const UniversalId custom_id = IdAllocatorType::INVALID_ID) {
        return emplace_field(m_fields.end(), field, custom_id);
    }

    void emplace_fields(const FieldListType::iterator &pos, size_t amount, EType default_type = EType::I64);

    std::expected<void, std::string> remove_field_by_name(const std::string &field_name);

    std::expected<void, std::string> remove_field_by_pos(const FieldListType::iterator &pos);

    std::expected<void, std::string> remove_field_by_id(UniversalId id);

    std::expected<void, std::string> rename_field_by_pos(const FieldListType::iterator &pos,
                                                         const std::string &new_name);

    std::expected<void, std::string> rename_field_by_name(const std::string &field_name, const std::string &new_name);

    std::expected<void, std::string> rename_field_by_id(UniversalId id, const std::string &new_name);

    ALWAYS_INLINE void emplace_back_fields(const size_t amount, const EType default_type = EType::I64) {
        emplace_fields(m_fields.end(), amount, default_type);
    }

    FieldListType::iterator obtain_field_by_name(const std::string &field_name);

    FieldListType::iterator obtain_field_by_id(UniversalId field_id);

    void splice_field_to_pos(const FieldListType::iterator &from, const FieldListType::iterator &to);

    ALWAYS_INLINE FieldListType::iterator begin_fields() { return m_fields.begin(); }

    ALWAYS_INLINE FieldListType::iterator end_fields() { return m_fields.end(); }

    ALWAYS_INLINE FieldListType &fields() { return m_fields; }

    ALWAYS_INLINE FieldListType::iterator range_begin() const { return begin; }

    ALWAYS_INLINE FieldListType::iterator range_end() const { return end; }

    void clear_fields();

    [[nodiscard]] std::shared_ptr<StructView> obtain_struct_view() const;

    void deregister_struct_view() const;

    void serialize(nlohmann::json &json);

    void deserialize(const nlohmann::json &json);

    template <typename... TArgs>
    std::string make_imgui_id(const std::string_view other, TArgs &&...args) {
        return fmt::format("##{}_{}_StructData_{}", ui_view_id, name,
                           fmt::vformat(other, fmt::make_format_args(args...)));
    }
};

template <typename... TArgs>
std::string FieldBase::make_imgui_id(const std::string_view other, TArgs &&...args) {
    return fmt::format("##{}_{}_{}_{}_{}_{}", id, name, parent->name, address, offset,
                       fmt::vformat(other, fmt::make_format_args(args...)));
}
