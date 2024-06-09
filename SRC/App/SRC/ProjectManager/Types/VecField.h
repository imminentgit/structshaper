#pragma once
#include "PodField.h"

struct ComponentEntry {
    std::string name{};

    EType type{};

    std::string value{};
};

class VecField : public FieldBase {
public:
    using ComponentVector = std::vector<ComponentEntry>;

private:
    ComponentVector m_components{};

    size_t m_memory_size{};

    bool m_are_all_the_same_type = false;

    bool m_collapse_type = true;
public:
    size_t optional_ordering{};

    size_t optional_rows{};

    size_t optional_cols{};

    VecField() : FieldBase(EFieldClassType::VecField, default_pod_type_by_current_process()){};
    VecField(const EType vec_type, ComponentVector component_vector, const size_t memory_size,
             const bool are_all_the_same_type = true) :
        FieldBase(EFieldClassType::VecField, vec_type), m_components(std::move(component_vector)),
        m_memory_size(memory_size), m_are_all_the_same_type(are_all_the_same_type) {}

    void render(bool is_offscreen) override;

    size_t memory_size() override;

    std::string type_name() override;

    void serialize(nlohmann::json &json) override;

    void deserialize(const nlohmann::json &json) override;

    [[nodiscard]] ALWAYS_INLINE const ComponentVector &components() const { return m_components; }

    void reset_field() override;

    std::shared_ptr<FieldBase> clone() override;

    static std::shared_ptr<VecField> make_vec2_field(EType pod_type);

    static std::shared_ptr<VecField> make_vec3_field(EType pod_type);

    static std::shared_ptr<VecField> make_vec4_field(EType pod_type);

    static std::shared_ptr<VecField> make_euler_field(EType pod_type, EEulerOrdering ordering);

    static std::shared_ptr<VecField> make_color_field(EType pod_type, EColorType ordering);

    static std::shared_ptr<VecField> make_quaternion_field(EType pod_type);

    static std::shared_ptr<VecField> make_matrix_field(EType pod_type, size_t rows, size_t cols, bool is_row_major);
};
