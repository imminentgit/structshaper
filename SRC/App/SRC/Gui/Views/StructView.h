#pragma once
#include "BaseView.h"

#include <nlohmann/json.hpp>

#include <ProjectManager/Types/PodField.h>

class Struct;
class StructView : public BaseView {
    std::string m_address_string{};

    std::string m_add_field_popup_id{};
    std::string m_struct_context_menu_popup{};

    int m_add_field_amount = 1024;

    EType m_selected_pod_type = EType::I64;

    void render_add_field_popup();

    [[nodiscard]] Struct &obtain_struct_data() const;
public:
    StructView();

    bool begin() override;

    void render() override;

    void update_address();

    void serialize(nlohmann::json &json);

    void deserialize(const nlohmann::json &json);

    bool begin_docked_callback_inner() override;

    void end_docked_callback_inner() override;
};