#include "ProcessInterface.h"

#include <ProjectManager/ProjectManager.h>

#include <Gui/Views/Popups/PopupView.h>
#include <Gui/Views/Popups/SelectProcessInterfacePopup.h>
#include <Gui/Views/Popups/SelectProcessPopup.h>

#include <Logger/Logger.h>

#include <Queue/Event.h>

#include <Utils/expected_result.h>

FORCE_CLASS_TO_BE_INSTANTIATED(ProcessInterface);

struct RTTICompleteObjectLocator {
    unsigned long signature;
    unsigned long offset;
    unsigned long cdOffset;
    int pTypeDescriptor;  // Image relative offset of TypeDescriptor
    int pClassDescriptor; // Image relative offset of _RTTIClassHierarchyDescriptor
    int pSelf;            // Image relative offset of this object
};

struct RTTIClassHierarchyDescriptor {
    unsigned long signature;
    unsigned long attributes;
    unsigned long numBaseClasses;
    int pBaseClassArray; // Image relative offset of _RTTIBaseClassArray
};

ProcessInterface::ProcessInterface() {
    Event::ProjectLoaded::listen(
        [this](const ProjectInfo &project_info) {
            if (!std::filesystem::exists(project_info.process_interface_path)) {
                PopupView::show_popup(popup_SelectProcessInterfacePopup);
                return;
            }

            // TODO: Make event system use expected
            MUST(load_process_interface_from_path(project_info.process_interface_path));
        },
        0);

    Event::ProjectUnloaded::listen([this] { MUST(unload_process_interface()); });

    Event::OnMenuBarRender::listen(
        [this] {
            const auto has_current_interface = current_loaded_interface() != nullptr;

            if (ImGui::BeginMenu("Process interface")) {
                if (ImGui::MenuItem("Select new interface")) {
                    PopupView::show_popup(popup_SelectProcessInterfacePopup);
                }

                if (has_current_interface) {
                    if (ImGui::MenuItem("Unload")) {
                        MUST(unload_process_interface());
                    }

                    if (ImGui::MenuItem("Select a process")) {
                        PopupView::show_popup(popup_SelectProcessPopup);
                    }

                    if (m_current_process.is_attached() && ImGui::MenuItem("Detach from process")) {
                        MUST(m_current_process.detach());
                    }
                }

                ImGui::EndMenu();
            }
        },
        1);

    Event::OnMenuBarRender::listen([this] {
        if (current_loaded_interface() && m_current_process.is_attached()) {
            ImGui::Separator();
            ImGui::Text("Process: %s", m_current_process.name.c_str());
            ImGui::Text("PID: %d", m_current_process.pid);
            ImGui::Text("32 bit: %s", m_current_process.is_64_bit ? "No" : "Yes");
        }
    });
}

ProcessInterface::~ProcessInterface() {
    if (m_process_interface_library_handle) {
        if (m_current_process.is_attached()) {
            if (!m_current_process.detach()) {
                LOG_WARN("Failed detaching from process on ProcessInterface destruction");
            }
        }

        if (!unload_process_interface()) {
            LOG_WARN("Failed unloading process interface on ProcessInterface destruction");
        }
    }
}

std::expected<void, std::string> ProcessInterface::load_process_interface_from_path(const std::string_view path) {
    if (m_process_interface_library_handle) {
        if (const auto res = unload_process_interface(); !res) {
            return std::unexpected(res.error());
        }
    }

#ifdef STRUCTSHAPER_IS_WINDOWS
    m_process_interface_library_handle = LoadLibraryA(path.data());
    if (!m_process_interface_library_handle) {
        return unexpected_ec(GetLastError(), "Failed to load process interface library");
    }

    m_fn_init_interface = reinterpret_cast<decltype(m_fn_init_interface)>(
        GetProcAddress(m_process_interface_library_handle, "init_interface"));
    if (!m_fn_init_interface) {
        return unexpected_ec(GetLastError(), "Exported init_interface function not found");
    }

    m_fn_shutdown_interface = reinterpret_cast<decltype(m_fn_shutdown_interface)>(
        GetProcAddress(m_process_interface_library_handle, "shutdown_interface"));
    if (!m_fn_shutdown_interface) {
        return unexpected_ec(GetLastError(), "Exported shutdown_interface function not found");
    }
#elif STRUCTSHAPER_IS_LINUX
    m_process_interface_library_handle = dlopen(path.data(), RTLD_LAZY);
    if (!m_process_interface_library_handle) {
        throw formatted_error("dlopen failed: {}", dlerror());
    }

    m_fn_init_interface =
        reinterpret_cast<decltype(m_fn_init_interface)>(dlsym(m_process_interface_library_handle, "init_interface"));
    if (!m_fn_init_interface) {
        throw formatted_error("Exported init_interface function not found: {}", dlerror());
    }

    m_fn_shutdown_interface = reinterpret_cast<decltype(m_fn_shutdown_interface)>(
        dlsym(m_process_interface_library_handle, "shutdown_interface"));
    if (!m_fn_shutdown_interface) {
        throw formatted_error("Exported shutdown_interface function not found: {}", dlerror());
    }
#endif

    m_process_interface = m_fn_init_interface();
    if (!m_process_interface) {
        return std::unexpected("Failed to initialize process interface");
    }

    m_process_interface_path = path;
    LOG_INFO("Loaded process interface from path: {} with description: {}", path,
             m_process_interface->interface_description());

    return {};
}

std::expected<void, std::string> ProcessInterface::unload_process_interface() {
    if (!m_process_interface_library_handle) {
        return {};
    }

    if (!m_fn_shutdown_interface) {
        return std::unexpected("Exported shutdown_interface function is nullptr");
    }

    m_fn_shutdown_interface();
#ifdef STRUCTSHAPER_IS_WINDOWS
    if (!FreeLibrary(m_process_interface_library_handle)) {
        return unexpected_ec(GetLastError(), "Failed to free process interface library");
    }
#elif STRUCTSHAPER_IS_LINUX
    if (dlclose(m_process_interface_library_handle)) {
        throw formatted_error("dlclose failed: {}", dlerror());
    }
#endif

    m_fn_init_interface = nullptr;
    m_fn_shutdown_interface = nullptr;
    m_process_interface = nullptr;
    m_process_interface_library_handle = nullptr;

    LOG_INFO("Unloaded process interface from path: {}", m_process_interface_path);
    m_process_interface_path.clear();

    return {};
}

std::expected<void, std::string> ProcessContext::attach(uint32_t pid, const std::string_view name) {
    const auto &process_interface = ProcessInterface::the();
    auto *loaded_interface = process_interface.current_loaded_interface();
    if (!loaded_interface) {
        return std::unexpected("No process interface loaded");
    }

    if (handle) {
        return detach();
    }

    handle = TRY(loaded_interface->open_process(pid));

    this->pid = pid;
    this->name = name;
    is_64_bit = !TRY(loaded_interface->is_process_32_bit(handle));

    image_base_64 = TRY(loaded_interface->get_base_address(handle, false));
    if (!is_64_bit) {
        image_base_32 = TRY(loaded_interface->get_base_address(handle, true));
    }

    // TODO: Add auto-update
    TRY(loaded_interface->get_modules(handle, native_modules));

    auto regions = TRY(loaded_interface->get_virtual_memory(handle));
    for (const auto &region : regions) {
        memory_regions[region.address + region.size] = region;
    }

    LOG_INFO("image_base_64 = {:#X}, image_base_32 = {:#X}", image_base_64, image_base_32);
    LOG_INFO("Attached to process with pid: {} and name: {}", pid, name);

    return {};
}

std::expected<void, std::string> ProcessContext::detach() {
    const auto &process_interface = ProcessInterface::the();
    auto *loaded_interface = process_interface.current_loaded_interface();
    if (!loaded_interface) {
        throw std::runtime_error("No process interface loaded");
    }

    TRY(loaded_interface->close_process(handle));
    *this = {};

    return {};
}

NativeModuleEntry *ProcessContext::get_module_from_address(const uintptr_t address) {
    auto it = native_modules.address_to_module.lower_bound(address);
    if (it != native_modules.address_to_module.end()) {
        auto *module_data = it->second;
        if (module_data->address <= address) {
            return module_data;
        }
    }

    return nullptr;
}

std::optional<MemoryRegion> ProcessContext::get_memory_region_from_address(const uintptr_t address) {
    auto found = memory_regions.lower_bound(address);
    if (found != memory_regions.end()) {
        const auto &region = found->second;
        if (region.address <= address) {
            return region;
        }
    }

    return std::nullopt;

}

void ProcessContext::get_indirections_for_address(uintptr_t address,
                                                                std::vector<uintptr_t> &out_indirections,
                                                                const size_t max_indirections) {
    const auto &process_interface = ProcessInterface::the();
    auto *loaded_interface = process_interface.current_loaded_interface();
    if (!loaded_interface) {
        return;
    }

    while (out_indirections.size() < max_indirections) {
        uintptr_t next_address{};
        loaded_interface->read_process_memory(handle, address, &next_address, get_pointer_size());

        if (!get_memory_region_from_address(next_address)) {
            break;
        }

        address = next_address;
        out_indirections.emplace_back(address);
    };
}

//TODO: Check if char is ascii or we crash
std::expected<std::string, std::string> ProcessContext::get_rtti_name(uintptr_t address, NativeModuleEntry *module) {
    const auto &process_interface = ProcessInterface::the();
    auto *loaded_interface = process_interface.current_loaded_interface();
    if (!loaded_interface) {
        return std::unexpected("No process interface loaded");
    }

    // TODO: Add lookup table here

    const auto target_ptr_size = get_pointer_size();

    uintptr_t rtti_object_locator_address{};
    auto res = loaded_interface->read_process_memory(handle, address - target_ptr_size, &rtti_object_locator_address,
                                                     target_ptr_size);
    if (!res) {
        return unexpected_fmt("Failed to read memory for RTTI object locator address: {}", res.error());
    }

    if (!is_valid_address(rtti_object_locator_address)) {
        return unexpected_fmt("Invalid RTTI object locator address: 0x{:#X}", rtti_object_locator_address);
    }

    RTTICompleteObjectLocator rtti_object_locator{};
    res = loaded_interface->read_process_memory(handle, rtti_object_locator_address, &rtti_object_locator,
                                                sizeof(rtti_object_locator));
    if (!res) {
        return unexpected_fmt("Failed to read memory for RTTI object locator: {}", res.error());
    }

    auto class_descriptor_base =
        is_64_bit ? module->address + rtti_object_locator.pClassDescriptor : rtti_object_locator.pClassDescriptor;

    if (!is_valid_address(class_descriptor_base)) {
        return unexpected_fmt("Invalid RTTI class descriptor address: 0x{:#X}", class_descriptor_base);
    }

    RTTIClassHierarchyDescriptor rtti_class_descriptor{};
    res = loaded_interface->read_process_memory(handle, class_descriptor_base, &rtti_class_descriptor,
                                                sizeof(rtti_class_descriptor));

    if (!res) {
        return unexpected_fmt("Failed to read memory for RTTI class descriptor: {}", res.error());
    }

    const auto base_class_arr_addr =
        is_64_bit ? module->address + rtti_class_descriptor.pBaseClassArray : rtti_class_descriptor.pBaseClassArray;

    if (!is_valid_address(base_class_arr_addr)) {
        return unexpected_fmt("Invalid RTTI base class array address: 0x{:#X}", base_class_arr_addr);
    }

    std::string out_name{};
    for (size_t i = 0; i < rtti_class_descriptor.numBaseClasses; i++) {
        unsigned long base_class_offset{};

        auto base_class_array_item_pointer_to_addr = base_class_arr_addr + i * sizeof(base_class_offset);

        if (!is_valid_address(base_class_array_item_pointer_to_addr)) {
            continue;
        }

        res = loaded_interface->read_process_memory(handle, base_class_array_item_pointer_to_addr, &base_class_offset,
                                                    sizeof(base_class_offset));

        if (!res) {
            // LOG_ERROR("Failed to read memory for base class offset: {}", res.error());
            break;
        }

        auto type_descriptor_pointer_to_addr = is_64_bit ? module->address + base_class_offset : base_class_offset;

        if (!is_valid_address(type_descriptor_pointer_to_addr)) {
            continue;
        }

        unsigned long type_descriptor_offset{};
        res = loaded_interface->read_process_memory(handle, type_descriptor_pointer_to_addr, &type_descriptor_offset,
                                                    sizeof(type_descriptor_offset));

        if (!res) {
            // LOG_ERROR("Failed to read memory for type descriptor offset: {}", res.error());
            break;
        }

        auto type_descriptor_addr = is_64_bit ? module->address + type_descriptor_offset : type_descriptor_offset;
        if (!is_valid_address(type_descriptor_addr)) {
            continue;
        }

        auto type_name_begin = type_descriptor_addr + (is_64_bit ? 0x10 : 0x8);

        std::vector<char> type_name(256, '\0');
        bool end_found = false;

        while (!end_found) {
            res = loaded_interface->read_process_memory(handle, type_name_begin, type_name.data(), type_name.size());
            if (!res) {
                break;
            }

            auto found = std::find(type_name.begin(), type_name.end(), '\0');
            if (found != type_name.end()) {
                end_found = true;
                type_name.resize(found - type_name.begin());
            }
            else {
                type_name_begin += type_name.size();
            }
        }

        std::string mangled_name{type_name.data(), type_name.size()};
        if (mangled_name.starts_with(".?")) {
            mangled_name = "??_7" + mangled_name.substr(2) + "6B@";
        }

        out_name += Utils::demangle_name(mangled_name);

        if (i + 1 < rtti_class_descriptor.numBaseClasses) {
            out_name += ": ";
        }
    }

    return out_name;
}

ProcessInterface::InterfaceVec ProcessInterface::grab_interfaces() {
    InterfaceVec interfaces{};
    for (auto &iface : std::filesystem::directory_iterator(INTERFACES_PATH)) {
        const auto &path = iface.path();
        if (!iface.is_regular_file()
#ifdef STRUCTSHAPER_IS_WINDOWS
            || path.extension() != ".dll"
#else
            || path.extension() != ".so"
#endif
        ) {
            continue;
        }

        interfaces.emplace_back(path.filename().string(), path.string());
    }
    return interfaces;
}
