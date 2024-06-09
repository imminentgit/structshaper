#pragma once
#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using native_type_t = void *;

struct NativeProcessEntry {
    static constexpr auto INVALID_ID = static_cast<size_t>(-1);

    std::string name{};

    // Can be invalid
    size_t parent_id = INVALID_ID;

    size_t sequence_number{};
};
using NativeProcessEntryMapType = std::unordered_map<size_t, NativeProcessEntry>;

struct NativeSection {
    uintptr_t address{};

    size_t size{};

    uint32_t characteristics{};
};

struct NativeModuleExport {
    std::string name{};

    uintptr_t address{};

    uint32_t ordinal;

    bool is_forwarder = false;
    std::string forwarder_name{};
};

struct NativeModuleEntry {
    std::string name{};

    uintptr_t address{};

    size_t size{};

    bool is_32_bit_module = false;

    std::string path{};

    std::map<std::string, NativeSection> sections{};
    std::map<std::string, NativeModuleExport> exports{};
};

struct NativeModulesInfo {
    using NativeModuleEntryMapType = std::unordered_map<std::string, NativeModuleEntry>;
    using NativeModuleAddressMapType = std::map<uintptr_t, NativeModuleEntry*>;

    NativeModuleEntryMapType modules{};
    NativeModuleAddressMapType address_to_module{};
};

struct MemoryRegion {
    uintptr_t address{};
    uintptr_t size{};
};

struct ProcessInterfaceBase {
    ProcessInterfaceBase() = default;
    virtual ~ProcessInterfaceBase() = default;

    [[nodiscard]] virtual std::string interface_description() const = 0;

    [[nodiscard]] virtual std::expected<native_type_t, std::string> open_process(uint32_t pid) = 0;

    virtual std::expected<void, std::string> close_process(native_type_t process_handle) = 0;

    [[nodiscard]] virtual std::expected<NativeProcessEntryMapType, std::string>
    get_processes(const std::unordered_set<size_t> &pid_filter) = 0;

    [[nodiscard]] virtual std::expected<bool, std::string> is_process_32_bit(native_type_t process_handle) = 0;

    virtual std::expected<size_t, std::string> read_process_memory(native_type_t process_handle, uintptr_t address,
                                                                   void *buffer, size_t buffer_size) = 0;
    virtual std::expected<size_t, std::string> write_process_memory(native_type_t process_handle, uintptr_t address,
                                                                    void *buffer, size_t buffer_size) = 0;

    [[nodiscard]] virtual std::expected<uintptr_t, std::string> get_base_address(native_type_t process_handle,
                                                                                 bool get_32_bit_base) = 0;

    // Used to check if modules need to be grabbed again
    virtual std::expected<size_t, std::string> get_module_count(native_type_t process_handle) = 0;

    virtual std::expected<void, std::string> get_modules(native_type_t process_handle, NativeModulesInfo& out) = 0;

    virtual std::expected<std::vector<MemoryRegion>, std::string> get_virtual_memory(native_type_t process_handle) = 0;
};
