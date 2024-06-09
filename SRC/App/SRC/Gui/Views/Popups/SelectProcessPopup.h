#pragma once
#include "PopupView.h"

#include <unordered_map>

#include <ProcessInterfaceBase.h>

class SelectProcessPopup : public PopupView {
    using ProcessGraphType = std::unordered_map<size_t, std::vector<size_t>>;

    NativeProcessEntryMapType m_processes{};

    ProcessGraphType m_process_graph{};

    std::string m_search_text{};

    void build_process_graph();

    void draw_node(size_t pid, size_t level);

    void draw_graph();
public:
    SelectProcessPopup();

    void render() override;

    void on_buttons_add() override;
};

REGISTER_POPUP(SelectProcessPopup);
