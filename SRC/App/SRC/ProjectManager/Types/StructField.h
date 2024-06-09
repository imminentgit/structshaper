#pragma once
#include <utility>

#include "FieldBase.h"

EType default_pod_type_by_current_process();

class StructField : public FieldBase {
    std::string m_other_struct_name{};

    size_t m_rename_event_idx{}, m_event_update_idx{}, m_field_removed_idx{};
    size_t m_memory_size{};

    bool m_did_first_update = false;

    std::map<UniversalId, std::shared_ptr<FieldBase>> m_fields{};

    void update_fields();

    void draw_fields();
public:
    [[nodiscard]] Struct *obtain_other_struct_data() const;

    StructField(std::string other_struct_name = {});

    ~StructField() override;

    void render(bool is_offscreen) override;

    size_t memory_size() override;

    std::string type_name() override;

    void serialize(nlohmann::json &json) override;

    void deserialize(const nlohmann::json &json) override;

    void reset_field() override;

    void copy_data(std::shared_ptr<FieldBase> to_field) override;
};
