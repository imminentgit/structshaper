#pragma once
#include "FieldBase.h"

EType default_pod_type_by_current_process();

class PodField : public FieldBase, public std::enable_shared_from_this<PodField> {
public:
    std::string field_value_str{};
private:
    std::expected<void, std::string> write_field_value_from_string(std::string_view str);

    std::expected<std::string, std::string> field_value_to_string();

    void render_pod_field(bool is_named_mode);
public:
    PodField() : FieldBase(EFieldClassType::PodField, default_pod_type_by_current_process()) {}

    PodField(const EType pod_type) : FieldBase(EFieldClassType::PodField, pod_type) {}

    void render(bool is_offscreen) override;

    size_t memory_size() override;

    void reset_field() override;

    void copy_data(std::shared_ptr<FieldBase> to_field) override;

    std::shared_ptr<FieldBase> clone() override;

    static std::shared_ptr<PodField> make_default_pod_field();
};

using DefaultFieldType = PodField;
