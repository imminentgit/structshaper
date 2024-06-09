#include <expected>
#ifdef STRUCTSHAPER_IS_WINDOWS
    #include <ntstatus.h>
    #include <windows.h>
    #include "win/nt_defs.h"

    #include <ProcessInterfaceBase.h>

    #include <Logger/Logger.h>

    #include <Utils/ScopeGuard.h>
    #include <Utils/Utils.h>
    #include <Utils/expected_result.h>

struct ProcessInterface_UserMode : ProcessInterfaceBase {
    uintptr_t m_minimum_address{};

    ProcessInterface_UserMode() {
        SYSTEM_INFO sys_info{};
        GetSystemInfo(&sys_info);

        m_minimum_address = reinterpret_cast<uintptr_t>(sys_info.lpMinimumApplicationAddress);

        nt::build_syscall_table();
    }

    static std::expected<uintptr_t, std::string> get_peb(const native_type_t process_handle,
                                                         const bool get_32_bit_base) {
        if (get_32_bit_base) {
            ULONG_PTR peb32{};
            const auto res = nt::sys_NtQueryInformationProcess(process_handle, ProcessWow64Information, &peb32,
                                                               sizeof(peb32), nullptr);
            if (!nt::is_success(res)) {
                return unexpected_fmt("NtQueryInformationProcess failed: {}", nt::to_string(res));
            }

            return peb32;
        }

        nt::PROCESS_BASIC_INFORMATION info{};
        const auto res =
            NtQueryInformationProcess(process_handle, ProcessBasicInformation, &info, sizeof(info), nullptr);
        if (!nt::is_success(res)) {
            return unexpected_fmt("NtQueryInformationProcess failed: {}", nt::to_string(res));
        }

        return reinterpret_cast<uintptr_t>(info.PebBaseAddress);
    }

    [[nodiscard]] std::string interface_description() const override {
        return "Usermode process interface using ReadProcessMemory and WriteProcessMemory.";
    }

    static std::expected<native_type_t, std::string> open_process_impl(const size_t access, const uint32_t pid) {
        CLIENT_ID id = {};
        id.UniqueProcess = reinterpret_cast<void *>(pid);

        OBJECT_ATTRIBUTES attr = {};
        attr.Length = sizeof(attr);

        HANDLE handle{};
        const auto res = nt::sys_NtOpenProcess(&handle, access, &attr, &id);
        if (!nt::is_success(res)) {
            return unexpected_fmt("NtOpenProcess failed: {}", nt::to_string(res));
        }

        return handle;
    }

    std::expected<native_type_t, std::string> open_process(const uint32_t pid) override {
        return open_process_impl(PROCESS_ALL_ACCESS, pid);
    }

    std::expected<void, std::string> close_process(const native_type_t process_handle) override {
        const auto res = nt::sys_NtClose(process_handle);
        if (!nt::is_success(res)) {
            return unexpected_fmt("NtClose failed: {}", nt::to_string(res));
        }

        return {};
    }

    std::expected<NativeProcessEntryMapType, std::string>
    get_processes(const std::unordered_set<size_t> &pid_filter) override {
        ULONG buffer_size = 0x4000;
        auto buffer = std::make_unique<uint8_t[]>(buffer_size);

        NTSTATUS res;
        while (true) {
            constexpr auto SystemProcessInformation = 5;
            res = nt::sys_NtQuerySystemInformation(SystemProcessInformation, buffer.get(), buffer_size, &buffer_size);

            if (res == STATUS_BUFFER_TOO_SMALL || res == STATUS_INFO_LENGTH_MISMATCH) {
                buffer = std::make_unique<uint8_t[]>(buffer_size);
            }
            else {
                break;
            }
        }

        if (!nt::is_success(res)) {
            return unexpected_fmt("NtQuerySystemInformation failed: {}", nt::to_string(res));
        }

        NativeProcessEntryMapType out{};
        auto *process = reinterpret_cast<nt::SYSTEM_PROCESS_INFORMATION *>(buffer.get());
        do {
            const auto pid = reinterpret_cast<size_t>(process->UniqueProcessId);
            if (!pid_filter.empty() && pid_filter.contains(pid)) {
                continue;
            }

            NativeProcessEntry entry{};
            const auto parent_id = reinterpret_cast<size_t>(process->InheritedFromUniqueProcessId);
            entry.parent_id =
                !pid_filter.empty() && pid_filter.contains(parent_id) ? NativeProcessEntry::INVALID_ID : parent_id;
            if (process->ImageName.Buffer) {
                entry.name = Utils::wstring_to_string(process->ImageName.to_str_view());
            }

            try {
                if (const auto proc_handle_res = open_process_impl(PROCESS_ALL_ACCESS, pid)) {
                    constexpr auto ProcessSequenceNumber = 92;
                    constexpr auto ProcessTelemetryIdInformation = 64;

                    ULONGLONG sequence_number{};

                    auto status = nt::sys_NtQueryInformationProcess(*proc_handle_res, ProcessSequenceNumber,
                                                                    &sequence_number, sizeof(sequence_number), nullptr);

                    if (status == STATUS_INVALID_INFO_CLASS) {
                        nt::PROCESS_TELEMETRY_ID_INFORMATION telemetry{};
                        status = nt::sys_NtQueryInformationProcess(*proc_handle_res, ProcessTelemetryIdInformation,
                                                                   &telemetry, sizeof(telemetry), nullptr);

                        if (status == STATUS_BUFFER_OVERFLOW) {
                            status = STATUS_SUCCESS;
                        }

                        sequence_number = telemetry.ProcessSequenceNumber;
                    }

                    if (NT_SUCCESS(status)) {
                        entry.sequence_number = sequence_number;
                    }

                    if (auto closed = close_process(*proc_handle_res); !closed) {
                        LOG_WARN("Failed to close process handle (to get the sequence number) for pid {}: {}", pid,
                                 closed.error());
                    }
                }
            }
            catch (const std::exception &e) {
                LOG_ERROR("Failed to open process to get sequence number for process {}: {}", pid, e.what());
            }

            out.emplace(pid, entry);
        }
        while ((process = process->next()));

        return out;
    }

    std::expected<bool, std::string> is_process_32_bit(const native_type_t process_handle) override {
        // TODO: Implement this via a direct syscall
        BOOL is_wow64{};
        if (!IsWow64Process(process_handle, &is_wow64)) {
            return unexpected_ec(nt::user_last_error(), "IsWow64Process failed");
        }

        return is_wow64;
    }

    std::expected<size_t, std::string> read_process_memory(const native_type_t process_handle, const uintptr_t address,
                                                           void *buffer, const size_t buffer_size) override {
        SIZE_T read_bytes = 0;

        const auto res = nt::sys_NtReadVirtualMemory(process_handle, reinterpret_cast<LPCVOID>(address), buffer,
                                                     buffer_size, &read_bytes);
        if (!nt::is_success(res)) {
            return unexpected_fmt("NtReadVirtualMemory failed: {}", nt::to_string(res));
        }

        return read_bytes;
    }

    std::expected<size_t, std::string> write_process_memory(const native_type_t process_handle, const uintptr_t address,
                                                            void *buffer, const size_t buffer_size) override {
        SIZE_T written_bytes = 0;
        const auto res = nt::sys_NtWriteVirtualMemory(process_handle, reinterpret_cast<LPCVOID>(address), buffer,
                                                      buffer_size, &written_bytes);
        if (!nt::is_success(res)) {
            return unexpected_fmt("NtWriteVirtualMemory failed: {}", nt::to_string(res));
        }

        return written_bytes;
    }

    std::expected<uintptr_t, std::string> get_base_address(const native_type_t process_handle,
                                                           const bool get_32_bit_base) override {
        if (get_32_bit_base) {
            const auto peb32 = get_peb(process_handle, true);
            if (!peb32) {
                return unexpected_fmt("Failed to get PEB32: {}", peb32.error());
            }

            nt::PEB_EXTENDED<uint32_t> peb32_struct{};
            const auto res = read_process_memory(process_handle, *peb32, &peb32_struct, sizeof(peb32_struct));
            if (!res) {
                return unexpected_fmt("Failed to read PEB32: {}", res.error());
            }

            return peb32_struct.ImageBaseAddress;
        }

        const auto peb64 = get_peb(process_handle, false);
        if (!peb64) {
        }

        nt::PEB_EXTENDED peb64_struct{};
        const auto res = read_process_memory(process_handle, *peb64, &peb64_struct, sizeof(peb64_struct));
        if (!res) {
            return unexpected_fmt("Failed to read PEB64: {}", res.error());
        }

        return peb64_struct.ImageBaseAddress;
    }

    uintptr_t get_nt_header_address(const native_type_t process_handle, const uintptr_t base_address,
                                    IMAGE_DOS_HEADER &dos_header) {
        const auto res = read_process_memory(process_handle, base_address, &dos_header, sizeof(dos_header));
        if (!res) {
            return 0;
        }

        return base_address + dos_header.e_lfanew;
    }

    template <typename T = uint64_t>
    std::expected<void, std::string> read_peb(
        const native_type_t process_handle, const uintptr_t peb_address,
        const std::function<void(const nt::PEB_EXTENDED<T> &peb, const nt::LDR_DATA_TABLE_ENTRY<T> &ldr_data_entry)>
            &callback) {
        if (!peb_address) {
            return std::unexpected("PEB address is null");
        }

        auto peb = nt::PEB_EXTENDED<T>{};
        auto res = read_process_memory(process_handle, peb_address, &peb, sizeof(peb));
        if (!res) {
            return std::unexpected(res.error());
        }

        auto ldr = nt::PEB_LDR_DATA<T>{};
        res = read_process_memory(process_handle, peb.Ldr, &ldr, sizeof(ldr));
        if (!res) {
            return std::unexpected(res.error());
        }

        auto head = ldr.InMemoryOrderModuleList;
        const T head_address = peb.Ldr + offsetof(nt::PEB_LDR_DATA<T>, InMemoryOrderModuleList);

        for (T current = head.Flink; current && current != head_address;) {
            T table_address = current - offsetof(nt::LDR_DATA_TABLE_ENTRY<T>, InMemoryOrderLinks);
            if (!table_address) {
                continue;
            }

            auto next_read = nt::LIST_ENTRY<T>{};
            res = read_process_memory(process_handle, current, &next_read, sizeof(next_read));
            if (!res) {
                LOG_INFO("Failed to read next entry: {}", res.error());
                continue;
            }

            current = next_read.Flink;

            auto entry = nt::LDR_DATA_TABLE_ENTRY<T>{};
            res = read_process_memory(process_handle, table_address, &entry, sizeof(entry));
            if (!res) {
                LOG_INFO("Failed to read entry: {}", res.error());
                continue;
            }

            callback(peb, entry);
        }

        return {};
    }

    template <typename T = uint64_t>
    void resolve_forwarders(NativeModulesInfo::NativeModuleEntryMapType &map) {
        for (auto &[module_name, module_data] : map) {
            for (auto &[export_name, export_data] : module_data.exports) {
                if (!export_data.is_forwarder) {
                    continue;
                }

                const auto first_dot_pos = export_data.forwarder_name.find_first_of('.');
                if (!first_dot_pos) {
                    LOG_WARN("Failed to find first dot in forwarded export, skipping: {}!", export_data.forwarder_name);
                    continue;
                }

                auto forwarded_export_name = export_data.forwarder_name.substr(first_dot_pos + 1);
                auto forwarded_module_name = export_data.forwarder_name.substr(0, first_dot_pos) + ".dll";
                std::ranges::transform(forwarded_module_name, forwarded_module_name.begin(), ::tolower);

                if (forwarded_export_name[0] == '#') {
                    continue;
                }

                auto forwarded_module = map.find(forwarded_module_name);
                if (forwarded_module == map.end()) {
                    LOG_WARN("[WARNING] Failed to find forwarded module, skipping: {}!", forwarded_module_name);
                    continue;
                }

                auto forwarded_export = forwarded_module->second.exports.find(forwarded_export_name);
                if (forwarded_export == forwarded_module->second.exports.end()) {
                    LOG_WARN("[WARNING] Failed to find forwarded export, skipping: {}!", forwarded_export_name);
                    continue;
                }

                export_data.address = forwarded_export->second.address;
            }
        }
    }

    template <typename T = uint64_t>
    void process_modules(const native_type_t process_handle, NativeModulesInfo::NativeModuleEntryMapType &map) {
        for (auto &[module_name, module_data] : map) {
            IMAGE_DOS_HEADER dos_header{};
            const auto header_address = get_nt_header_address(process_handle, module_data.address, dos_header);
            if (!header_address) {
                continue;
            }

            if (dos_header.e_magic != IMAGE_DOS_SIGNATURE || !dos_header.e_lfanew || dos_header.e_lfanew > 0x10000000) {
                return;
            }

            auto nt_headers = std::conditional_t<sizeof(T) == 8, IMAGE_NT_HEADERS64, IMAGE_NT_HEADERS32>();
            if (const auto res = read_process_memory(process_handle, header_address, &nt_headers, sizeof(nt_headers));
                !res) {
                continue;
            }

            if (nt_headers.Signature != IMAGE_NT_SIGNATURE) {
                continue;
            }

            for (size_t i = 0; i < nt_headers.FileHeader.NumberOfSections; ++i) {
                IMAGE_SECTION_HEADER header{};
                const auto section_address = header_address + sizeof(nt_headers) + i * sizeof(header);
                if (const auto res = read_process_memory(process_handle, section_address, &header, sizeof(header));
                    !res) {
                    continue;
                }

                if (!strlen(reinterpret_cast<const char *>(header.Name))) {
                    continue;
                }

                const auto section_name = reinterpret_cast<const char *>(header.Name);
                NativeSection section{};
                section.address = module_data.address + header.VirtualAddress;
                section.size = header.Misc.VirtualSize;
                section.characteristics = header.Characteristics;

                module_data.sections.emplace(section_name, section);
            }

            auto export_data_dir = nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            if (export_data_dir.Size) {
                auto buf = std::make_unique<uint8_t[]>(export_data_dir.Size);
                const auto res =
                    read_process_memory(process_handle, module_data.address + export_data_dir.VirtualAddress, buf.get(),
                                        export_data_dir.Size);
                if (!res) {
                    continue;
                }

                auto *export_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY *>(buf.get());
                auto *ordinals = reinterpret_cast<uint16_t *>(
                    &buf.get()[export_dir->AddressOfNameOrdinals - export_data_dir.VirtualAddress]);
                auto *names = reinterpret_cast<uint32_t *>(
                    &buf.get()[export_dir->AddressOfNames - export_data_dir.VirtualAddress]);
                auto *functions = reinterpret_cast<uint32_t *>(
                    &buf.get()[export_dir->AddressOfFunctions - export_data_dir.VirtualAddress]);

                for (size_t i = 0; i < export_dir->NumberOfNames; ++i) {
                    auto *export_name =
                        reinterpret_cast<const char *>(&buf.get()[names[i] - export_data_dir.VirtualAddress]);

                    auto ordinal = ordinals[i];
                    auto rva = functions[ordinal];

                    NativeModuleExport export_entry{};
                    export_entry.name = export_name;

                    if (rva >= export_data_dir.VirtualAddress &&
                        rva < export_data_dir.VirtualAddress + export_data_dir.Size) {
                        export_entry.is_forwarder = true;
                        export_entry.forwarder_name = (char *)(&buf.get()[rva - export_data_dir.VirtualAddress]);
                    }

                    export_entry.ordinal = ordinal + export_dir->Base;
                    export_entry.address = module_data.address + rva;

                    module_data.exports.emplace(export_name, export_entry);
                }
            }
        }

        resolve_forwarders<T>(map);
    }

    std::expected<size_t, std::string> get_module_count(native_type_t process_handle) override {
        const auto peb32 = get_peb(process_handle, true);
        if (!peb32) {
            LOG_WARN("Failed to get PEB32: {}", peb32.error());
        }

        const auto peb64 = get_peb(process_handle, false);
        if (!peb64) {
            return unexpected_fmt("Failed to get PEB64: {}", peb64.error());
        }

        size_t count = 0;
        if (peb32) {
            const auto res = read_peb<uint32_t>(process_handle, *peb32,
                                                [&](const nt::PEB_EXTENDED<uint32_t> &peb,
                                                    const nt::LDR_DATA_TABLE_ENTRY<uint32_t> &entry) { ++count; });
            if (!res) {
                return std::unexpected(res.error());
            }
        }

        if (peb64) {
            const auto res = read_peb<uint64_t>(process_handle, *peb32,
                                                [&](const nt::PEB_EXTENDED<uint64_t> &peb,
                                                    const nt::LDR_DATA_TABLE_ENTRY<uint64_t> &entry) { ++count; });

            if (!res) {
                return std::unexpected(res.error());
            }
        }

        return count;
    }

    template <typename T = uint64_t>
    std::expected<void, std::string> process_peb(native_type_t process_handle, const uintptr_t peb,
                                                 NativeModulesInfo &out) {
        return read_peb<T>(process_handle, peb,
                           [&](const nt::PEB_EXTENDED<T> &peb, const nt::LDR_DATA_TABLE_ENTRY<T> &entry) {
                               std::wstring dll_path(entry.FullDllName.Length / sizeof(wchar_t), L'\0');
                               const auto res = read_process_memory(process_handle, entry.FullDllName.Buffer,
                                                                    dll_path.data(), entry.FullDllName.Length);
                               if (!res) {
                                   LOG_INFO("Failed to read dll path: {}", res.error());
                                   return;
                               }

                               std::ranges::transform(dll_path, dll_path.begin(), ::tolower);

                               auto first_backslash = dll_path.find_last_of(L'\\');
                               if (first_backslash == std::wstring::npos) {
                                   return;
                               }

                               auto dll_name = dll_path.substr(first_backslash + 1);
                               if (!(dll_name.ends_with(L".dll") || dll_name.ends_with(L".exe"))) {
                                   return;
                               }

                               NativeModuleEntry module{};
                               module.name = Utils::wstring_to_string(dll_name);
                               module.address = entry.DllBase;
                               module.size = entry.SizeOfImage;
                               module.is_32_bit_module = sizeof(T) == sizeof(uint32_t);
                               module.path = Utils::wstring_to_string(dll_path);

                               out.modules.emplace(module.name, module);
                               out.address_to_module.emplace(module.address + module.size, &out.modules[module.name]);
                           });
    }

    std::expected<void, std::string> get_modules(native_type_t process_handle, NativeModulesInfo &out) override {
        auto is_32_bit = is_process_32_bit(process_handle);
        if (!is_32_bit) {
            return unexpected_fmt("Failed to get process bitness: {}", is_32_bit.error());
        }

        const auto peb32 = get_peb(process_handle, true);
        if (!peb32) {
            LOG_WARN("Failed to get PEB32: {}", peb32.error());
        }

        const auto peb64 = get_peb(process_handle, false);
        if (!peb64) {
            return unexpected_fmt("Failed to get PEB64: {}", peb64.error());
        }

        if (peb32 && *peb32) {
            const auto res = process_peb<uint32_t>(process_handle, *peb32, out);
            if (res) {
                process_modules<uint32_t>(process_handle, out.modules);
            }
        }

        if (!*is_32_bit && peb64 && *peb64) {
            const auto res = process_peb<uint64_t>(process_handle, *peb64, out);
            if (!res) {
                return unexpected_fmt("Failed to read PEB64: {}", res.error());
            }

            process_modules<uint64_t>(process_handle, out.modules);
        }

        return {};
    }

    std::expected<std::vector<MemoryRegion>, std::string> get_virtual_memory(native_type_t process_handle) override {
        std::vector<MemoryRegion> regions{};
        MEMORY_BASIC_INFORMATION info{};
        uintptr_t address = m_minimum_address;

        while (true) {
            SIZE_T buf_bytes{};
            auto res = nt::sys_NtQueryVirtualMemory(process_handle, reinterpret_cast<void *>(address), &info,
                                                    sizeof(info), &buf_bytes);
            if (nt::is_success(res) && buf_bytes > 0) {
                if (info.State == MEM_COMMIT) {
                    MemoryRegion region{};
                    region.address = address;
                    region.size = info.RegionSize;
                    regions.push_back(region);
                }
                address = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
            }
            else {
                break;
            }
        }

        return regions;
    }
};

static ProcessInterface_UserMode *instance = nullptr;

extern "C" __declspec(dllexport) ProcessInterfaceBase *init_interface() {
    if (!instance) {
        instance = new ProcessInterface_UserMode();
    }
    return instance;
}

extern "C" __declspec(dllexport) void shutdown_interface() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}
#endif
