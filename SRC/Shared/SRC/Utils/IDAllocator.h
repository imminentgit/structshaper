#pragma once
#include <vector>
#include <set>

#include <nlohmann/json.hpp>

#include <Defines.h>

using UniversalId = size_t;

template <size_t Base = 0>
class IdAllocator {
    using IdVectorType = std::vector<UniversalId>;
    IdVectorType _free_ids = {};
    UniversalId _last_field_id = Base;
public:
    static constexpr UniversalId INVALID_ID = 0;

    [[nodiscard]] UniversalId allocate() {
        UniversalId field_id;

        if (!_free_ids.empty()) {
            field_id = _free_ids.back();
            _free_ids.pop_back();
        } else {
            field_id = ++_last_field_id;
        }

        return field_id;
    }

    ALWAYS_INLINE void free(const UniversalId field_id) {
        _free_ids.emplace_back(field_id);
    }

    ALWAYS_INLINE void remove_free_id(const UniversalId field_id) {
        _free_ids.erase(std::remove(_free_ids.begin(), _free_ids.end(), field_id), _free_ids.end());
    }

    ALWAYS_INLINE void clear() {
        _last_field_id = Base;
        _free_ids.clear();
    }

    void serialize(nlohmann::json& json) {
        json["last_field_id"] = _last_field_id;
        json["free_ids"] = _free_ids;
    }

    void deserialize(const nlohmann::json& json, const bool only_free_ids = false) {
        if (only_free_ids) {
            _free_ids = json.at("free_ids").get<IdVectorType>();
            return;
        }

        _last_field_id = json.at("last_field_id");
        _free_ids = json.at("free_ids").get<IdVectorType>();
    }
};