#pragma once

template<typename T>
struct StaticInstance {
    static T &the() {
        static T instance{};
        return instance;
    }

    // Prevent assignment
    StaticInstance& operator=(const StaticInstance&) = delete;
};