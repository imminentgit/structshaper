#include "nt_defs.h"

#include <stdexcept>
#include <unordered_map>

#include <Logger/Logger.h>

#include "Utils/Fnv1.h"

namespace nt {
    static std::unordered_map<Utils::Fnv1::HashType, uint32_t> s_syscall_table;

    void build_syscall_table() {
        auto *peb = reinterpret_cast<PEB_EXTENDED<> *>(__readgsqword(0x60));
        if (!peb || !peb->Ldr) {
            throw std::runtime_error("Failed to read PEB");
        }

        auto *module_list = &reinterpret_cast<PEB_LDR_DATA<> *>(peb->Ldr)->InMemoryOrderModuleList;
        if (!module_list) {
            throw std::runtime_error("Failed to read module list");
        }

        static uintptr_t export_dir = 0, dll_base = 0;
        if (!export_dir || !dll_base) {
            for (auto *i = reinterpret_cast<LIST_ENTRY<> *>(module_list->Flink); i != module_list;
                 i = reinterpret_cast<LIST_ENTRY<> *>(i->Flink)) {
                const auto ldr_entry = CONTAINING_RECORD(i, LDR_DATA_TABLE_ENTRY<>, InMemoryOrderLinks);
                auto *image_dos_header = reinterpret_cast<IMAGE_DOS_HEADER *>(ldr_entry->DllBase);
                if (!image_dos_header || image_dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
                    LOG_WARN("Skipping invalid module at base 0x{:X}", ldr_entry->DllBase);
                    continue;
                }

                auto *nt_image_headers = reinterpret_cast<IMAGE_NT_HEADERS *>(
                    reinterpret_cast<uintptr_t>(image_dos_header) + image_dos_header->e_lfanew);
                if (!nt_image_headers || nt_image_headers->Signature != IMAGE_NT_SIGNATURE) {
                    LOG_WARN("Skipping invalid module at base 0x{:X}", ldr_entry->DllBase);
                    continue;
                }

                auto const *str = reinterpret_cast<const wchar_t *>(ldr_entry->BaseDllName.Buffer);
                if (Utils::Fnv1::hash(str) == FNV1("ntdll.dll")) {
                    export_dir =
                        nt_image_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                    dll_base = ldr_entry->DllBase;
                    break;
                }
            }
        }

        if (!export_dir || !dll_base) {
            throw std::runtime_error("Failed to find ntdll.dll");
        }

        const auto *dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY *>(dll_base + export_dir);
        const auto *ordinals = reinterpret_cast<uint16_t *>(dll_base + dir->AddressOfNameOrdinals);
        const auto *names = reinterpret_cast<uint32_t *>(dll_base + dir->AddressOfNames);
        const auto *functions = reinterpret_cast<uint32_t *>(dll_base + dir->AddressOfFunctions);

        for (size_t i = 0; i < dir->NumberOfNames; ++i) {
            const auto *export_name = reinterpret_cast<const char *>(dll_base + names[i]);
            auto export_addr = dll_base + functions[ordinals[i]];

            const auto is_valid_export = (export_name[0] == 'Z' && export_name[1] == 'w') || (export_name[0] == 'N' && export_name[1] == 't');
            if (!is_valid_export) {
                // LOG_WARN("Skipping non-Zw syscall: {}", export_name);
                continue;
            }

            // Syscall id must be on the top of the function
            auto did_find = false;
            for (size_t j = 0; j < 32; ++j) {
                if (reinterpret_cast<uint8_t *>(export_addr)[j] == 0xB8) {
                    export_addr += j + 1;
                    did_find = true;
                    break;
                }
            }

            if (!did_find) {
                // LOG_WARN("Failed to find syscall id for {}", export_name);
                continue;
            }

            const auto syscall_id = *reinterpret_cast<uint32_t *>(export_addr);
            s_syscall_table[Utils::Fnv1::hash(export_name)] = syscall_id;
            //LOG_INFO("Added syscall: {} -> 0x{:X}", export_name, syscall_id);
        }
    }

    // Dislike the idea of throwing, maybe change later?
    NTSTATUS sys_NtQueryInformationProcess(const HANDLE ProcessHandle, const SIZE_T ProcessInformationClass,
                                           const PVOID ProcessInformation, const ULONG ProcessInformationLength,
                                           const PULONG ReturnLength) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtQueryInformationProcess"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtQueryInformationProcess syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, HANDLE, SIZE_T, PVOID, ULONG, PULONG)>(&do_syscall)(
            syscall_id->second, ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength,
            ReturnLength);
    }

    NTSTATUS sys_NtQuerySystemInformation(_In_ const ULONG SystemInformationClass,
                                          _Out_writes_bytes_opt_(SystemInformationLength) const PVOID SystemInformation,
                                          _In_ const ULONG SystemInformationLength,
                                          _Out_opt_ const PULONG ReturnLength) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtQuerySystemInformation"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtQuerySystemInformation syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, ULONG, PVOID, ULONG, PULONG)>(&do_syscall)(
            syscall_id->second, SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
    }

    NTSTATUS sys_NtOpenProcess(const PHANDLE ProcessHandle, const ACCESS_MASK DesiredAccess,
                               const POBJECT_ATTRIBUTES ObjectAttributes, CLIENT_ID *ClientId) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtOpenProcess"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtOpenProcess syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, CLIENT_ID *)>(
            &do_syscall)(syscall_id->second, ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
    }

    NTSTATUS sys_NtClose(const HANDLE Handle) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtClose"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtClose syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, HANDLE, PVOID, PVOID, PVOID)>(&do_syscall)(
            syscall_id->second, Handle, nullptr, nullptr, nullptr);
    }

    NTSTATUS sys_NtReadVirtualMemory(const HANDLE ProcessHandle, const LPCVOID BaseAddress, const LPVOID Buffer,
                                     const SIZE_T Size, SIZE_T *NumberOfBytesRead) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtReadVirtualMemory"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtReadVirtualMemory syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T *)>(&do_syscall)(
            syscall_id->second, ProcessHandle, BaseAddress, Buffer, Size, NumberOfBytesRead);
    }

    NTSTATUS sys_NtWriteVirtualMemory(HANDLE ProcessHandle, LPCVOID BaseAddress, LPVOID Buffer, SIZE_T Size,
                                      SIZE_T *NumberOfBytesWritten) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtWriteVirtualMemory"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtWriteVirtualMemory syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T *)>(&do_syscall)(
            syscall_id->second, ProcessHandle, BaseAddress, Buffer, Size, NumberOfBytesWritten);
    }

    NTSTATUS sys_NtQueryVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
                                      PVOID MemoryInformation, SIZE_T MemoryInformationLength, PSIZE_T ReturnLength) {
        static const auto syscall_id = s_syscall_table.find(FNV1("NtQueryVirtualMemory"));
        if (syscall_id == s_syscall_table.end()) {
            throw std::runtime_error("Failed to find NtQueryVirtualMemory syscall");
        }

        return reinterpret_cast<NTSTATUS (*)(size_t, HANDLE, PVOID, DWORD, PVOID, SIZE_T, PSIZE_T)>(&do_syscall)(
            syscall_id->second, ProcessHandle, BaseAddress, 0, MemoryInformation, MemoryInformationLength, ReturnLength);
    }

    // https://web.archive.org/web/20150121053632/http://support.microsoft.com/kb/259693
    std::string to_string(const NTSTATUS status) {
        LPVOID lpMessageBuffer = nullptr;

        const auto handle = LoadLibraryA("ntdll.dll");
        if (!handle) {
            throw std::runtime_error("Failed to get ntdll.dll handle");
        }

        const auto size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE, handle, status,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&lpMessageBuffer), 0, nullptr);
        if (size == 0) {
            throw fmt::system_error(user_last_error(), "FormatMessageA failed");
        }

        std::string message(static_cast<char *>(lpMessageBuffer), size);
        LocalFree(lpMessageBuffer);
        return message;
    }
} // namespace nt
