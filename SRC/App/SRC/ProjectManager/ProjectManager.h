#pragma once

#include <string>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <Utils/StaticInstance.h>

#include "Struct.h"

struct ProjectInfo {
    std::string name{};
    std::string path{};
    std::string description{};
    std::string process_interface_path{};

    std::chrono::system_clock::duration last_modified{};

    void serialize(nlohmann::json &json) const {
        json["description"] = description;
        json["process_interface_path"] = process_interface_path;
    }

    void deserialize(const nlohmann::json &json) {
        description = json.value("description", "");
        process_interface_path = json.value("process_interface_path", "");
    }
};

struct ProjectManager : StaticInstance<ProjectManager> {
    inline static auto PROJECTS_PATH =
#ifdef STRUCTSHAPER_DEV
        "../../../Projects";
#else
        "Projects";
#endif

    ProjectManager();

    ProjectInfo project_info{};
    std::map<std::string, Struct> structs{};

    bool is_dirty = false;

    void mark_as_dirty(bool dirty = true);

    void rename_struct(const std::string &old_name, const std::string &new_name);

    void create_struct(const std::string& name);

    void delete_struct(const std::string& name);

    void create_project(const ProjectInfo &new_project_info);

    void load_project(std::string_view name);

    void save_project();

    void close_project(bool close_after_save = false);

    [[nodiscard]] bool is_project_loaded() const { return !project_info.name.empty(); }

    static std::string project_path(const std::string_view name) {
        return fmt::format("{}/{}.sproj", PROJECTS_PATH, name);
    }
};
