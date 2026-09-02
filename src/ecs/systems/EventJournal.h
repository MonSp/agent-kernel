#pragma once
// EventJournal — append-only event log with ring buffer storage.
// Thread-safe: all public methods are mutex-protected.

#include "../Entity.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <mutex>

namespace Systems {

struct JournalEvent {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    ECS::EntityId entity_id = 0;
    std::string event_type;
    std::string payload;

    JournalEvent() = default;
    JournalEvent(uint64_t id, uint64_t ts, ECS::EntityId eid,
                 const std::string& type, const std::string& data)
        : id(id), timestamp(ts), entity_id(eid), event_type(type), payload(data) {}
};

class EventJournal {
public:
    using Listener = std::function<void(const JournalEvent&)>;

    explicit EventJournal(size_t max_capacity = 10000)
        : capacity_(max_capacity), next_id_(1), head_(0), count_(0) {
        buffer_.resize(capacity_);
    }

    void addListener(Listener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.push_back(std::move(listener));
    }

    uint64_t append(ECS::EntityId entity_id, const std::string& event_type,
                    const std::string& payload) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        uint64_t id = next_id_++;
        buffer_[head_] = JournalEvent(id, static_cast<uint64_t>(now),
                                       entity_id, event_type, payload);
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) count_++;

        for (auto& listener : listeners_) {
            listener(buffer_[(head_ + capacity_ - 1) % capacity_]);
        }
        return id;
    }

    std::vector<JournalEvent> query(ECS::EntityId entity_id, uint64_t since_id = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<JournalEvent> result;
        forEach([&](const JournalEvent& e) {
            if (e.entity_id == entity_id && e.id >= since_id) {
                result.push_back(e);
            }
        });
        return result;
    }

    std::vector<JournalEvent> queryAll(uint64_t since_id = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<JournalEvent> result;
        forEach([&](const JournalEvent& e) {
            if (e.id >= since_id) {
                result.push_back(e);
            }
        });
        return result;
    }

    uint64_t latestId() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return next_id_ - 1;
    }

    size_t size() const { std::lock_guard<std::mutex> lock(mutex_); return count_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { std::lock_guard<std::mutex> lock(mutex_); return count_ == 0; }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        count_ = 0;
    }

private:
    template<typename Fn>
    void forEach(Fn&& fn) const {
        if (count_ == 0) return;
        size_t start = (count_ < capacity_) ? 0 : head_;
        for (size_t i = 0; i < count_; ++i) {
            size_t idx = (start + i) % capacity_;
            fn(buffer_[idx]);
        }
    }

    size_t capacity_;
    uint64_t next_id_;
    size_t head_;
    size_t count_;
    std::vector<JournalEvent> buffer_;
    std::vector<Listener> listeners_;
    mutable std::mutex mutex_;
};

} // namespace Systems
