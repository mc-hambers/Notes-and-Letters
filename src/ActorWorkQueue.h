#pragma once
#include <deque>
#include <unordered_set>
#include <cstdint>

namespace NL {
// Runtime-only IDs: no engine pointers survive a queued job or a save reload.
class ActorWorkQueue {
    std::deque<std::uint32_t> pending;
    std::unordered_set<std::uint32_t> present;
public:
    void add(std::uint32_t id) { if (id && present.insert(id).second) pending.push_back(id); }
    std::uint32_t pop() {
        if (pending.empty()) return 0;
        auto id=pending.front();pending.pop_front();present.erase(id);return id;
    }
    bool empty() const { return pending.empty(); }
    void clear() { pending.clear();present.clear(); }
};
}
