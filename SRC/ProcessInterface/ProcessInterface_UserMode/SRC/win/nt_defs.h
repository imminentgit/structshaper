#pragma once
#ifdef STRUCTSHAPER_IS_WINDOWS
    #include <string_view>
    #include <windows.h>
    #include <winternl.h>

    #include <cstdint>

// syscall idx, rcx, rdx, r8, r9, stack
extern "C" NTSTATUS do_syscall();

// Yes, I can read the required fields via offsets, or undefine the other fields as reserved fields, but I'm not going to do that
namespace nt {
    using KPRIORITY = LONG;

    template <typename T = uintptr_t>
    struct UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        _Field_size_bytes_part_opt_(MaximumLength, Length) T Buffer;

        [[nodiscard]] std::wstring_view to_str_view() const {
            return {reinterpret_cast<wchar_t *>(Buffer), Length / sizeof(wchar_t)};
        }
    };

    struct SYSTEM_THREAD_INFORMATION {
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER CreateTime;
        ULONG WaitTime;
        ULONG_PTR StartAddress;
        CLIENT_ID ClientId;
        KPRIORITY Priority;
        KPRIORITY BasePriority;
        ULONG ContextSwitches;
        ULONG ThreadState; // KTHREAD_STATE
        ULONG WaitReason;  // KWAIT_REASON
    };

    struct SYSTEM_PROCESS_INFORMATION {
        ULONG NextEntryOffset;
        ULONG NumberOfThreads;
        LARGE_INTEGER WorkingSetPrivateSize; // since VISTA
        ULONG HardFaultCount;                // since WIN7
        ULONG NumberOfThreadsHighWatermark;  // since WIN7
        ULONGLONG CycleTime;                 // since WIN7
        LARGE_INTEGER CreateTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER KernelTime;
        UNICODE_STRING<> ImageName;
        KPRIORITY BasePriority;
        HANDLE UniqueProcessId;
        HANDLE InheritedFromUniqueProcessId;
        ULONG HandleCount;
        ULONG SessionId;
        ULONG_PTR UniqueProcessKey; // since VISTA (requires SystemExtendedProcessInformation)
        SIZE_T PeakVirtualSize;
        SIZE_T VirtualSize;
        ULONG PageFaultCount;
        SIZE_T PeakWorkingSetSize;
        SIZE_T WorkingSetSize;
        SIZE_T QuotaPeakPagedPoolUsage;
        SIZE_T QuotaPagedPoolUsage;
        SIZE_T QuotaPeakNonPagedPoolUsage;
        SIZE_T QuotaNonPagedPoolUsage;
        SIZE_T PagefileUsage;
        SIZE_T PeakPagefileUsage;
        SIZE_T PrivatePageCount;
        LARGE_INTEGER ReadOperationCount;
        LARGE_INTEGER WriteOperationCount;
        LARGE_INTEGER OtherOperationCount;
        LARGE_INTEGER ReadTransferCount;
        LARGE_INTEGER WriteTransferCount;
        LARGE_INTEGER OtherTransferCount;
        SYSTEM_THREAD_INFORMATION Threads[1]; // SystemProcessInformation
        // SYSTEM_EXTENDED_THREAD_INFORMATION Threads[1]; // SystemExtendedProcessinformation
        // SYSTEM_EXTENDED_THREAD_INFORMATION + SYSTEM_PROCESS_INFORMATION_EXTENSION // SystemFullProcessInformation

        [[nodiscard]] SYSTEM_PROCESS_INFORMATION *next() {
            return NextEntryOffset
                ? reinterpret_cast<SYSTEM_PROCESS_INFORMATION *>(reinterpret_cast<uintptr_t>(this) + NextEntryOffset)
                : nullptr;
        }
    };

    struct PROCESS_BASIC_INFORMATION {
        NTSTATUS ExitStatus;
        PVOID PebBaseAddress;
        ULONG_PTR AffinityMask;
        INT BasePriority;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR InheritedFromUniqueProcessId;
    };

    struct PROCESS_TELEMETRY_ID_INFORMATION {
        ULONG HeaderSize;
        ULONG ProcessId;
        ULONGLONG ProcessStartKey;
        ULONGLONG CreateTime;
        ULONGLONG CreateInterruptTime;
        ULONGLONG CreateUnbiasedInterruptTime;
        ULONGLONG ProcessSequenceNumber;
        ULONGLONG SessionCreateTime;
        ULONG SessionId;
        ULONG BootId;
        ULONG ImageChecksum;
        ULONG ImageTimeDateStamp;
        ULONG UserSidOffset;
        ULONG ImagePathOffset;
        ULONG PackageNameOffset;
        ULONG RelativeAppNameOffset;
        ULONG CommandLineOffset;
    };


    // Those are from an old project
    template <typename T = uintptr_t>
    struct LIST_ENTRY {
        T Flink;
        T Blink;
    };

    template <typename T = uintptr_t>
    class PEB_LDR_DATA {
        uint8_t _padding_1[8];
        T _padding_2[3];

    public:
        LIST_ENTRY<T> InMemoryOrderModuleList;
    };

    template <typename T = uintptr_t>
    struct LDR_DATA_TABLE_ENTRY {
        LIST_ENTRY<T> InLoadOrderLinks;
        LIST_ENTRY<T> InMemoryOrderLinks;
        LIST_ENTRY<T> InInitializationOrderLinks;
        T DllBase;
        T EntryPoint;
        T SizeOfImage;
        UNICODE_STRING<T> FullDllName;
        UNICODE_STRING<T> BaseDllName;
    };

    template <typename T = uintptr_t>
    class PEB_EXTENDED {
    public:
        union {
            T _alignment1;
            struct {
                uint8_t InheritedAddressSpace;
                uint8_t ReadImageFileExecOptions;
                uint8_t BeingDebugged;
                uint8_t BitField;
            };
        };
        T Mutant;
        T ImageBaseAddress;
        T Ldr;

    private:
        T _padding1[0x6];

    public:
        union {
            T _alignment2;
            unsigned long CrossProcessFlags;
            struct {
                unsigned long ProcessInJob : 1, ProcessInitializing : 1, ProcessUsingVEH : 1, ProcessUsingVCH : 1,
                    ProcessUsingFTH : 1;
            };
        };

    private:
        T _padding2[0x1];
        uint8_t _padding3[0x8];

    public:
        T ApiSetMap;
        union {
            uint32_t _alignment3;
            unsigned long TlsExpansionCounter;
        };
        T TlsBitmap;
        unsigned long TlsBitmapBits[2];
    };

    void build_syscall_table();

    NTSTATUS sys_NtQueryInformationProcess(HANDLE ProcessHandle, SIZE_T ProcessInformationClass,
                                           PVOID ProcessInformation, ULONG ProcessInformationLength,
                                           PULONG ReturnLength);

    NTSTATUS sys_NtQuerySystemInformation(_In_ ULONG SystemInformationClass,
                                          _Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
                                          _In_ ULONG SystemInformationLength, _Out_opt_ PULONG ReturnLength);

    NTSTATUS sys_NtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
                               CLIENT_ID *ClientId);

    NTSTATUS sys_NtClose(HANDLE Handle);

    NTSTATUS sys_NtReadVirtualMemory(HANDLE ProcessHandle, LPCVOID BaseAddress, LPVOID Buffer, SIZE_T Size,
                                     SIZE_T *NumberOfBytesRead);

    NTSTATUS sys_NtWriteVirtualMemory(HANDLE ProcessHandle, LPCVOID BaseAddress, LPVOID Buffer, SIZE_T Size,
                                      SIZE_T *NumberOfBytesWritten);

    NTSTATUS sys_NtQueryVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
                                  PVOID MemoryInformation, SIZE_T MemoryInformationLength, PSIZE_T ReturnLength);

    std::string to_string(NTSTATUS status);

    inline int user_last_error() { return static_cast<int>(GetLastError()); }

    inline bool is_success(const NTSTATUS status) { return NT_SUCCESS(status); }
} // namespace nt

#endif
