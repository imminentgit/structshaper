#pragma once
#include "BaseView.h"

class StructsListView : public BaseView {
    std::string m_search_query{};
public:
    StructsListView();

    bool begin() override;

    void render() override;

    bool begin_docked_callback_inner() override;

    void end_docked_callback_inner() override;
};

REGISTER_VIEW(StructsListView);
