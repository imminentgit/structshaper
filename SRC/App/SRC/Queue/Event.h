#pragma once

#include <algorithm>
#include <functional>
#include <mutex>

#include <Gui/Gui.h>

#include <ProjectManager/ProjectManager.h>

namespace Event {
    template <typename T>
    struct EventEntry {
        using EventFunc = std::function<T>;

        int priority;
        EventFunc callback;
    };

#define DECLARE_EVENT(EventName, ...)                                                                                  \
    namespace EventName {                                                                                              \
        namespace Detail {                                                                                             \
            using EventEntry = EventEntry<void(__VA_ARGS__)>;                                                          \
            inline std::vector<EventEntry> s_event_callbacks;                                                          \
            inline std::mutex s_event_lock;                                                                            \
        }                                                                                                              \
                                                                                                                       \
        static void listen(Detail::EventEntry::EventFunc &&func, const int priority = -1) {                            \
            std::scoped_lock g(Detail::s_event_lock);                                                                  \
            auto &callbacks = Detail::s_event_callbacks;                                                               \
            callbacks.emplace_back(EventEntry{priority, func});                                                        \
            std::ranges::sort(callbacks, [](const Detail::EventEntry &a, const Detail::EventEntry &b) {                \
                if (a.priority == -1)                                                                                  \
                    return false;                                                                                      \
                if (b.priority == -1)                                                                                  \
                    return true;                                                                                       \
                                                                                                                       \
                return a.priority < b.priority;                                                                        \
            });                                                                                                        \
        }                                                                                                              \
                                                                                                                       \
        template <typename... TArgs>                                                                                   \
        static void call(TArgs &&...args) {                                                                            \
            std::scoped_lock g(Detail::s_event_lock);                                                                  \
            for (auto &[_, callback] : Detail::s_event_callbacks) {                                                    \
                callback(std::forward<TArgs>(args)...);                                                                \
            }                                                                                                          \
        }                                                                                                              \
    }

#define DECLARE_DEFERRED_EVENT(EventName, ...)                                                                         \
    namespace EventName {                                                                                              \
        namespace Detail {                                                                                             \
            using EventEntry = EventEntry<bool(__VA_ARGS__)>;                                                          \
            inline std::vector<EventEntry> s_event_callbacks;                                                          \
            inline std::mutex s_event_lock;                                                                            \
            inline std::stack<std::tuple<__VA_ARGS__>> s_event_queue;                                                  \
        }                                                                                                              \
        static size_t listen(Detail::EventEntry::EventFunc &&func, const int priority = -1) {                          \
            std::scoped_lock g(Detail::s_event_lock);                                                                  \
            auto &callbacks = Detail::s_event_callbacks;                                                               \
            const auto idx = Detail::s_event_callbacks.size();                                                         \
            callbacks.emplace_back(EventEntry{priority, func});                                                        \
            std::ranges::sort(callbacks, [](const Detail::EventEntry &a, const Detail::EventEntry &b) {                \
                if (a.priority == -1)                                                                                  \
                    return false;                                                                                      \
                if (b.priority == -1)                                                                                  \
                    return true;                                                                                       \
                return a.priority < b.priority;                                                                        \
            });                                                                                                        \
            return idx;                                                                                                \
        }                                                                                                              \
                                                                                                                       \
        static void unlisten(const size_t idx) {                                                                       \
            auto &callbacks = Detail::s_event_callbacks;                                                               \
            callbacks.erase(callbacks.begin() + static_cast<ptrdiff_t>(idx));                                          \
        }                                                                                                              \
                                                                                                                       \
        template <typename... TArgs>                                                                                   \
        static void call(TArgs &&...args) {                                                                            \
            std::scoped_lock g(Detail::s_event_lock);                                                                  \
            Detail::s_event_queue.emplace(std::forward<TArgs>(args)...);                                               \
        }                                                                                                              \
        static void update() {                                                                                         \
            std::scoped_lock g(Detail::s_event_lock);                                                                  \
            auto &queue = Detail::s_event_queue;                                                                       \
            if (!queue.empty()) {                                                                                      \
                auto args = std::move(queue.top());                                                                    \
                queue.pop();                                                                                           \
                for (auto &[_, callback] : Detail::s_event_callbacks) {                                                \
                    if (std::apply(callback, args)) {                                                                  \
                        break;                                                                                         \
                    }                                                                                                  \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        static bool update_pull_single(const Detail::EventEntry::EventFunc &func) {                                    \
            std::scoped_lock g(Detail::s_event_lock);                                                                  \
            auto &queue = Detail::s_event_queue;                                                                       \
            if (!queue.empty()) {                                                                                      \
                auto args = std::move(queue.top());                                                                    \
                queue.pop();                                                                                           \
                                                                                                                       \
                const auto res = std::apply(func, args);                                                               \
                for (auto &[_, callback] : Detail::s_event_callbacks) {                                                \
                    if (std::apply(callback, args)) {                                                                  \
                        break;                                                                                         \
                    }                                                                                                  \
                }                                                                                                      \
                                                                                                                       \
                return res;                                                                                            \
            }                                                                                                          \
            return false;                                                                                              \
        }                                                                                                              \
    }

    DECLARE_EVENT(ProjectLoaded, const ProjectInfo &);

    DECLARE_EVENT(ProjectUnloaded);

    DECLARE_EVENT(StructCreated, const std::string &);

    DECLARE_EVENT(OnMenuBarRender);

    DECLARE_EVENT(OnPreDraw, Gui &);

    DECLARE_EVENT(OnDraw, Gui &);

    DECLARE_EVENT(OnPostDraw, Gui &);

    DECLARE_DEFERRED_EVENT(UpdateFont, std::string, int);

    // Current struct, from, to
    DECLARE_DEFERRED_EVENT(MoveStruct, std::string, UniversalId, UniversalId);

    DECLARE_DEFERRED_EVENT(StructRename, std::string, std::string);

    DECLARE_DEFERRED_EVENT(StructDelete, std::string);

    // Current struct, field id
    DECLARE_DEFERRED_EVENT(RemoveField, std::string, UniversalId);

    // WARNING: call copy_data, if you want to swap an existing field, else wise emplace the new field first and then
    // swap
    DECLARE_DEFERRED_EVENT(SwapField, std::string, UniversalId, std::shared_ptr<FieldBase>);

    DECLARE_DEFERRED_EVENT(OnStructUpdate, std::string);
} // namespace Event
