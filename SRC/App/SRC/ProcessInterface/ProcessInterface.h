#pragma once
#ifdef STRUCTSHAPER_IS_WINDOWS
    #include <windows.h>
using dll_handle_t = HMODULE;
#elif STRUCTSHAPER_IS_LINUX
    #include <dlfcn.h>
using dll_handle_t = void *;
#endif

#include <optional>
#include <vector>

#include <ProcessInterfaceBase.h>

#include <Utils/StaticInstance.h>
#include <Utils/Utils.h>

#include <Defines.h>

struct ProcessContext {
    uint32_t pid{};

    std::string name{};

    native_type_t handle{};

    bool is_64_bit{};

    uintptr_t image_base_64{};

    uintptr_t image_base_32{};

    NativeModulesInfo native_modules{};

    std::map<uintptr_t, MemoryRegion> memory_regions{};

    [[nodiscard]] ALWAYS_INLINE uintptr_t image_base() const { return is_64_bit ? image_base_64 : image_base_32; }

    [[nodiscard]] ALWAYS_INLINE bool is_attached() const { return handle != nullptr; }

    std::expected<void, std::string> attach(uint32_t pid, std::string_view name);

    std::expected<void, std::string> detach();

    NativeModuleEntry *get_module_from_address(uintptr_t address);

    std::optional<MemoryRegion> get_memory_region_from_address(uintptr_t address);

    void get_indirections_for_address(uintptr_t address, std::vector<uintptr_t> &out_indirections,
                                                    size_t max_indirections = 10);

    std::expected<std::string, std::string> get_rtti_name(uintptr_t address, NativeModuleEntry * module);


    [[nodiscard]] ALWAYS_INLINE bool is_valid_address(const uintptr_t address) const {
        return is_64_bit ? address >= 0x10000 && address < 0x000F000000000000
                         : address >= 0x10000 && address < 0xFFF00000;
    }

    [[nodiscard]] ALWAYS_INLINE size_t get_pointer_size() const { return is_64_bit ? 8 : 4; }
};

struct InterfaceEntry {
    std::string filename{};
    std::string path{};
};

class ProcessInterface : public StaticInstance<ProcessInterface> {
    std::string m_process_interface_path{};

    dll_handle_t m_process_interface_library_handle = nullptr;

    ProcessInterfaceBase *m_process_interface = nullptr;

    // Callbacks to the dll
    ProcessInterfaceBase *(*m_fn_init_interface)() = nullptr;

    void (*m_fn_shutdown_interface)() = nullptr;

    ProcessContext m_current_process{};

public:
    using InterfaceVec = std::vector<InterfaceEntry>;

    inline static auto INTERFACES_PATH =
#ifdef STRUCTSHAPER_DEV
        "../../../Memory_Interfaces";
#else
        "Memory_Interfaces";
#endif

    ProcessInterface();

    ~ProcessInterface();

    std::expected<void, std::string> load_process_interface_from_path(std::string_view path);

    std::expected<void, std::string> unload_process_interface();

    [[nodiscard]] ALWAYS_INLINE ProcessContext &current_process() { return m_current_process; }

    [[nodiscard]] ALWAYS_INLINE ProcessInterfaceBase *current_loaded_interface() const { return m_process_interface; }

    [[nodiscard]] static InterfaceVec grab_interfaces();
};
