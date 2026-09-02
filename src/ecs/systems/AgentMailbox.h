#pragma once
// AgentMailbox — per-entity FIFO message queue. Thread-safe.

#include "../Entity.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <mutex>

namespace Systems {

struct MailboxMessage {
    uint64_t id = 0;
    ECS::EntityId from = 0;
    ECS::EntityId to = 0;
    std::string payload;
    uint64_t timestamp = 0;
    bool delivered = false;
    bool acked = false;

    MailboxMessage() = default;
    MailboxMessage(uint64_t id, ECS::EntityId from, ECS::EntityId to,
                   const std::string& payload, uint64_t ts)
        : id(id), from(from), to(to), payload(payload), timestamp(ts),
          delivered(false), acked(false) {}
};

class AgentMailbox {
public:
    using Listener = std::function<void(const MailboxMessage&)>;

    explicit AgentMailbox(size_t max_per_entity = 1000)
        : max_per_entity_(max_per_entity), next_id_(1) {}

    void addListener(Listener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.push_back(std::move(listener));
    }

    uint64_t send(ECS::EntityId from, ECS::EntityId to, const std::string& payload) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        uint64_t id = next_id_++;
        MailboxMessage msg(id, from, to, payload, static_cast<uint64_t>(now));

        auto& queue = queues_[to];
        if (queue.size() >= max_per_entity_) {
            queue.erase(queue.begin());
        }
        queue.push_back(msg);
        for (auto& listener : listeners_) {
            listener(msg);
        }
        return id;
    }

    std::vector<MailboxMessage> receive(ECS::EntityId to, int limit = 10) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<MailboxMessage> result;
        auto it = queues_.find(to);
        if (it == queues_.end()) return result;

        for (auto& msg : it->second) {
            if (!msg.delivered && static_cast<int>(result.size()) < limit) {
                msg.delivered = true;
                result.push_back(msg);
            }
        }
        return result;
    }

    bool ack(uint64_t message_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [eid, queue] : queues_) {
            for (auto& msg : queue) {
                if (msg.id == message_id) {
                    msg.acked = true;
                    return true;
                }
            }
        }
        return false;
    }

    int pendingCount(ECS::EntityId to) const {
        std::lock_guard<std::mutex> lock(mutex_);
        int count = 0;
        auto it = queues_.find(to);
        if (it != queues_.end()) {
            for (const auto& msg : it->second) {
                if (!msg.delivered) count++;
            }
        }
        return count;
    }

    std::vector<MailboxMessage> getAll(ECS::EntityId to) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(to);
        if (it == queues_.end()) return {};
        return it->second;
    }

    size_t totalMessages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& [eid, queue] : queues_) {
            total += queue.size();
        }
        return total;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queues_.clear();
    }

private:
    size_t max_per_entity_;
    uint64_t next_id_;
    std::unordered_map<ECS::EntityId, std::vector<MailboxMessage>> queues_;
    std::vector<Listener> listeners_;
    mutable std::mutex mutex_;
};

} // namespace Systems
