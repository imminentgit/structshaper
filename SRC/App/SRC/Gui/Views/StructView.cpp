#include "StructView.h"

#include "MessagesView.h"

#include <misc/cpp/imgui_stdlib.h>

#include <Gui/Gui.h>
#include <Gui/IconsLoader/IconsLoader.h>

#include <OptionsManager/OptionsManager.h>

#include <ProcessInterface/ProcessInterface.h>

#include <ProjectManager/ProjectManager.h>

#include <Logger/Logger.h>

#include <Utils/ScopeGuard.h>

#include <Queue/Event.h>

#ifdef STRUCTSHAPER_DEV

struct vec3_t {
    float x, y, z;
};

struct angle_t {
    float p, y, r;
};

struct IBasePlayer {
    vec3_t position{ 25.f, 121.f, 23.f };
    angle_t angle{ 5.f, 90.f, 0.f };

    int ammo = 30;
    int clip = 60;

    vec3_t velocity{ 5.f, 5.f, 8.f };

    virtual void update() = 0;
};

struct Player : IBasePlayer {
    void update() override {

    }
};
auto test_struct = std::make_unique<Player>();
#endif

StructView::StructView() : BaseView("StructView_REPLACE_ME", EViewType::MAIN_VIEW) { should_render = false; }

Struct &StructView::obtain_struct_data() const {
    auto &proj_mgr = ProjectManager::the();
    return proj_mgr.structs[title];
}

bool StructView::begin() {
    const auto &gui = Gui::the();
    ImGui::SetNextWindowDockID(gui.dockspace_id, ImGuiCond_Once);
    return BaseView::begin();
}

void StructView::render() {
    using namespace ImGui::Helpers;

    BaseView::render();

    const auto &gui = Gui::the();
    auto &struct_data = obtain_struct_data();

    m_add_field_popup_id = make_imgui_id("AddFieldPopup_{}", title);
    m_struct_context_menu_popup = make_imgui_id("StructContextMenuPopup_{}", title);

    const auto tree_size = ImGui::GetWindowSize();

    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

    ImGui::AlignTextToFramePadding();
    if (three_dot_button(title + "_StructTreeContext", Gui::menubar_wh())) {
        ImGui::OpenPopup(m_struct_context_menu_popup);
    }

    auto &process_interface = ProcessInterface::the();
    auto *proc_iface = process_interface.current_loaded_interface();

    if (ImGui::BeginPopup(m_struct_context_menu_popup)) {
        ScopeGuard context_menu_guard(&ImGui::EndPopup);
        ImGui::Checkbox("Consume fields", &struct_data.consume_fields);

        ImGui::Checkbox("Hide hex views on named fields", &struct_data.hide_hex_views_on_named_fields);

        ImGui::Checkbox("Show RTTI type info", &struct_data.show_rtti_type_info);

#ifdef STRUCTSHAPER_DEV
        ImGui::Checkbox("Show field's bounding box", &struct_data.show_debug_bb);

        // TODO: Make this nicer
        const auto PID =
#ifdef STRUCTSHAPER_IS_LINUX
                getpid();
#else
                GetCurrentProcessId();
#endif

        if (ImGui::Button("Set to TestStruct") && proc_iface) {
            if (process_interface.current_process().attach(PID, "App")) {
                 struct_data.address = reinterpret_cast<uintptr_t>(test_struct.get());
                //struct_data.address = reinterpret_cast<uintptr_t>(this);
                struct_data.update_field_offsets();

                m_address_string = fmt::format("0x{:X}", struct_data.address);
            }
        }
#endif
    }

    ImGui::SameLine();
    const auto tree_open = ImGui::TreeNodeEx(make_imgui_id("_StructTree"),
                                             ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::SameLine();
    m_address_string.empty()
        ? ImGui::SetNextItemWidth(get_char_width('?') * 20.f)
        : ImGui::SetNextItemWidth(ImGui::CalcTextSize(m_address_string).x + ImGui::GetStyle().FramePadding.x * 2.f);
    if (ImGui::InputText(make_imgui_id("_StructTree_AddressInput"), &m_address_string,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        update_address();
    }

    ImGui::PopStyleColor(3);

    ImGui::SameLine(tree_size.x - (20.f + ImGui::GetStyle().WindowPadding.x));

    if (ImGui::Button("+")) {
        ImGui::OpenPopup(m_add_field_popup_id);
    }

    render_add_field_popup();

    if (!tree_open) {
        return;
    }
    ScopeGuard tree_guard(&ImGui::TreePop);

    struct_data.struct_render_pos = ImGui::GetCursorScreenPos();

    Event::RemoveField::update_pull_single([&struct_data](const std::string &struct_name, const UniversalId id) {
        if (struct_data.name != struct_name) {
            return false;
        }

        const auto it = struct_data.obtain_field_by_id(id);
        if (it == struct_data.fields().end()) {
            return false;
        }

        const auto field_name = (*it)->name;

        if (const auto res = struct_data.remove_field_by_id(id); !res) {
            Gui::the().add_notification(fmt::format("Failed to remove field: {}", field_name), res.error());
        }

        Gui::the().add_notification("Removed field", field_name);
        return true;
    });

    Event::SwapField::update_pull_single([&struct_data](const std::string &struct_name, const UniversalId field_id,
                                                        std::shared_ptr<FieldBase> new_field) {
        if (struct_data.name != struct_name) {
            return false;
        }

        const auto field_iter = struct_data.obtain_field_by_id(field_id);
        if (field_iter == struct_data.fields().end()) {
            return false;
        }

        if (struct_data.consume_fields) {
            const auto current_field_size = (*field_iter)->memory_size();

            const auto new_field_size = new_field->memory_size();

            ptrdiff_t remaining_unallocated_space = 0;
            if (new_field_size > current_field_size) {
                // Destroy the adjacent fields until we have enough space
                ptrdiff_t extra_needed_space = new_field_size - current_field_size;
                auto it = field_iter;
                while (extra_needed_space > 0) {
                    it = std::next(it);
                    if (it == struct_data.fields().end()) {
                        return false;
                    }

                    auto &field_to_remove = *it;

                    extra_needed_space -= field_to_remove->memory_size();
                    Event::RemoveField::call(field_to_remove->parent->name, field_to_remove->id);
                }

                // Convert the unallocated space into unnamed fields
                if (extra_needed_space < 0) {
                    remaining_unallocated_space = -extra_needed_space;
                }
            }
            else if (new_field_size < current_field_size) {
                // Current field will get swapped, and convert the freed space into new unnamed fields
                remaining_unallocated_space = current_field_size - new_field_size;
            }

            // Allocate any remaining unallocated space
            if (remaining_unallocated_space > 0) {
                const auto biggest_pod_type = default_pod_type_by_current_process();

                const auto get_one_size_smaller_pod_type = [](EType pod_type) {
                    switch (pod_type) {
                    case EType::I64:
                        return EType::I32;
                    case EType::I32:
                        return EType::I16;
                    case EType::I16:
                        return EType::I8;
                    default:
                        return EType::I8;
                    }
                };

                auto last_inserted_it = std::next(field_iter);
                size_t remaining_space = remaining_unallocated_space;
                while (remaining_space > 0) {
                    EType fitting_pod_type = biggest_pod_type;
                    while (pod_type_size(fitting_pod_type) > remaining_space) {
                        fitting_pod_type = get_one_size_smaller_pod_type(fitting_pod_type);
                    }

                    size_t fitting_pod_type_size = pod_type_size(fitting_pod_type);
                    size_t num_fields = remaining_space / fitting_pod_type_size;
                    remaining_space -= num_fields * fitting_pod_type_size;
                    struct_data.emplace_fields(last_inserted_it, num_fields, fitting_pod_type);
                    // last_inserted_it = std::next(last_inserted_it, num_fields);
                }
            }
        }

        field_iter->swap(new_field);

        struct_data.update_everything();

        return true;
    });

    Event::OnStructUpdate::update();

    auto &struct_fields = struct_data.fields();
    const auto &style = ImGui::GetStyle();

    auto total_render_height = 0.f;
    std::ranges::for_each(struct_fields, [&](const std::shared_ptr<FieldBase> &field) {
        if (field->is_render_bounds_empty()) {
            field->update_render_bounds();
        }

        total_render_height += field->render_bounds.y + style.ItemSpacing.y;
    });

    ImGui::SetNextWindowContentSize({0.f, total_render_height});

    ImGui::BeginChild(make_imgui_id("_InnerStructView"), {}, 0, ImGuiWindowFlags_HorizontalScrollbar);
    ScopeGuard inner_struct_tree_view_guard(&ImGui::EndChild);

    struct_data.struct_render_bounds = ImGui::GetWindowSize();

    auto *monospace_font = gui.monospace_font;

    size_t mem_size = 0;
    for (const auto &field : struct_fields) {
        // TODO: Estimate amount of fields that need to be rendered and only render those that are visible on screen
        if (field->is_clipping_needed()) {
            ImGui::Dummy(field->render_bounds);
            continue;
        }

        ImGui::PushFont(monospace_font);
        field->render(false);
        ImGui::PopFont();

        const auto cursor = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            {cursor.x + 1.f, cursor.y - 2.f},
            {cursor.x + struct_data.struct_render_bounds.x + ImGui::GetScrollX(), cursor.y - 2.f},
            ImGui::GetColorU32(ImGuiCol_Separator));

        mem_size += field->memory_size();
    }

    struct_data.read_memory(mem_size);
}

void StructView::update_address() {
    try {
        auto &struct_data = obtain_struct_data();
        [&] {
            if (m_address_string == "base") {
                struct_data.address = ProcessInterface::the().current_process().image_base();
                return;
            }

            if (m_address_string.starts_with("base")) {
                bool add;
                auto operation_begin = m_address_string.find('+');
                if (operation_begin != std::string::npos) {
                    add = true;
                }
                else if ((operation_begin = m_address_string.find('-')) != std::string::npos) {
                    add = false;
                }
                else {
                    throw std::runtime_error("Invalid operation");
                }

                const auto after_operator = m_address_string.substr(operation_begin + 1);

                // TODO: Actually check if the string is hex
                const auto offset = std::stoull(after_operator, nullptr,
                                                after_operator.find_first_of("0x") != std::string::npos ? 16 : 10);
                struct_data.address = ProcessInterface::the().current_process().image_base() + (add ? offset : -offset);
                return;
            }

            struct_data.address = std::stoull(m_address_string, nullptr, 16);
        }();

        struct_data.update_field_offsets();
        m_address_string = fmt::format("0x{:X}", struct_data.address);
    }
    catch (const std::exception &e) {
        Gui::the().add_notification("Failed to set struct address", e.what());
        m_address_string.clear();
    }
}

void StructView::render_add_field_popup() {
    if (ImGui::BeginPopup(m_add_field_popup_id)) {
        if (ImGui::BeginCombo(m_add_field_popup_id + "_TypeCombo", to_string(m_selected_pod_type))) {
            const auto &options_mgr = OptionsManager::the();
            const auto &field_pod_types = options_mgr.options.field_pod_types;

            // TODO: Change this with the new system
            for (size_t i = 0; i < static_cast<size_t>(EType::COUNT_POD_TYPES); ++i) {
                if (ImGui::Selectable(field_pod_types[i])) {
                    m_selected_pod_type = static_cast<EType>(i);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Def")) {
            m_selected_pod_type = default_pod_type_by_current_process();
        }

        ImGui::Separator();

        ImGui::InputInt("##StructTree_FieldAmountInput", &m_add_field_amount, 1, 128);

        ImGui::SameLine();

        if (ImGui::Button("Add")) {
            obtain_struct_data().emplace_back_fields(m_add_field_amount, m_selected_pod_type);
        }
        ImGui::EndPopup();
    }
}

void StructView::serialize(nlohmann::json &json) {
    json["should_render"] = should_render;
    json["field_amount"] = m_add_field_amount;
}

void StructView::deserialize(const nlohmann::json &json) {
    should_render = json.value("should_render", false);
    m_add_field_amount = json.value("field_amount", 1024);

    m_address_string = fmt::format("0x{:X}", obtain_struct_data().address);
}

bool StructView::begin_docked_callback_inner() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, is_docked ? 0 : IM_COL32(16, 16, 16, 240));
    return true;
}

void StructView::end_docked_callback_inner() { ImGui::PopStyleColor(); }
