#ifdef STRUCTSHAPER_IS_LINUX
#include <ProcessInterfaceBase.h>

#include <fmt/args.h>
#include <fmt/os.h>

#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/uio.h>

#include <Logger/Logger.h>
#include <Utils/expected_result.h>

struct ProcessInterface_UserMode : ProcessInterfaceBase {
    ProcessInterface_UserMode() = default;

    std::string interface_description() const override {
        return "Usermode process interface using process_vm_read/writev.";
    }

    std::expected<native_type_t, std::string> open_process(const uint32_t pid) override {
        return reinterpret_cast<native_type_t>(pid);
    }

    std::expected<void, std::string> close_process(const native_type_t process_handle) override {
        return {};
    }

    std::expected<NativeProcessEntryMapType, std::string> get_processes(const std::unordered_set<size_t> &pid_filter) override {
        NativeProcessEntryMapType process_entries{};
        for (const auto& proc_entry : std::filesystem::directory_iterator("/proc")) {
            if (!proc_entry.is_directory()) {
                continue;
            }

            const auto filename = proc_entry.path().filename().string();
            const auto pid = std::strtol(filename.data(), nullptr, 10);
            if (pid == 0) {
                continue;
            }

            if (pid_filter.contains(pid)) {
                continue;
            }

            std::ifstream cmdline(fmt::format("/proc/{}/cmdline", pid));
            if (!cmdline.is_open()) {
                LOG_ERROR("Failed to open cmdline file for pid {}: {}", pid, strerror(errno));
                continue;
            }

            std::ifstream status(fmt::format("/proc/{}/status", pid));
            if (!status.is_open()) {
                LOG_ERROR("Failed to open status file for pid {}: {}", pid, strerror(errno));
                continue;
            }

            std::string status_line{};
            size_t parent_id = NativeProcessEntry::INVALID_ID;

            while (std::getline(status, status_line)) {
                if (status_line.find("PPid:") == 0) {
                    const auto pos = status_line.find(":");
                    if (pos != std::string::npos) {
                        parent_id = std::stoi(status_line.substr(pos + 1));
                    }
                }
            }
            std::string name{};
            std::getline(cmdline, name, '\0');

            NativeProcessEntry entry{};
            entry.name = std::move(name);
            entry.parent_id = parent_id;
            process_entries.emplace(pid, entry);
        }

        return process_entries;
    }

    std::expected<bool, std::string> is_process_32_bit(const native_type_t process_handle) override {
        const auto pid = reinterpret_cast<size_t>(process_handle);
        std::ifstream exepath(fmt::format("/proc/{}/exe", pid));
        if (!exepath.is_open()) {
            LOG_ERROR("Failed to open exe file for pid {}: {}", pid, strerror(errno));
            return false;
        }

        std::array<uint8_t, EI_NIDENT> elf_ident{};
        exepath.read(reinterpret_cast<char*>(elf_ident.data()), elf_ident.size());
        return elf_ident[EI_CLASS] == ELFCLASS32;
    }

    std::expected<size_t, std::string> read_process_memory(const native_type_t process_handle, const uintptr_t address, void *buffer,
                               const size_t buffer_size) override {
        const auto pid = reinterpret_cast<size_t>(process_handle);

        if (!buffer_size) {
            return std::unexpected("buffer_size can't be 0");
        }

        iovec local_address_space{};
        local_address_space.iov_base = buffer;
        local_address_space.iov_len = buffer_size;

        iovec remote_address_space{};
        remote_address_space.iov_base = reinterpret_cast<void*>(address);
        remote_address_space.iov_len = buffer_size;

        const ssize_t bytes_read = process_vm_readv(pid, &local_address_space, 1, &remote_address_space, 1, 0);
        if (bytes_read == -1) {
            return unexpected_fmt("process_vm_readv failed: {}", strerror(errno));
        }

        return bytes_read;
    }

    std::expected<size_t, std::string> write_process_memory(const native_type_t process_handle, const uintptr_t address, void *buffer,
                                const size_t buffer_size) override {
        const auto pid = reinterpret_cast<size_t>(process_handle);

        if (!buffer_size) {
            return std::unexpected("buffer_size can't be 0");
        }

        iovec local_address_space{};
        local_address_space.iov_base = buffer;
        local_address_space.iov_len = buffer_size;

        iovec remote_address_space{};
        remote_address_space.iov_base = reinterpret_cast<void*>(address);
        remote_address_space.iov_len = buffer_size;

        const ssize_t bytes_read = process_vm_writev(static_cast<int>(pid), &local_address_space, 1, &remote_address_space, 1, 0);
        if (bytes_read == -1) {
            return unexpected_fmt("process_vm_readv failed: {}", strerror(errno));
        }

        return bytes_read;
    }

    std::expected<uintptr_t, std::string> get_base_address(const native_type_t process_handle, const bool get_32_bit_base) override {
        // This is a pain since the maps file isn't that correct of a way.
        return 0;
    }

    std::expected<size_t, std::string> get_module_count(native_type_t process_handle) override {
        return std::unexpected("Not implemented");
    }

    std::expected<void, std::string> get_modules(native_type_t process_handle, NativeModulesInfo &out) override {
        return std::unexpected("Not implemented");
    }

    std::expected<std::vector<MemoryRegion>, std::string> get_virtual_memory(native_type_t process_handle) override {
        return std::unexpected("Not implemented");
    }
};

static ProcessInterface_UserMode *instance = nullptr;

extern "C" ProcessInterfaceBase *init_interface() {
    if (!instance) {
        instance = new ProcessInterface_UserMode();
    }
    return instance;
}

extern "C" void shutdown_interface() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}
#endif