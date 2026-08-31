#pragma once
#include <cstdint>
#include <cstring>

class EventStringPool {
public:
    static constexpr size_t MAX_EVENTS = 8192;
    static constexpr size_t MAX_STRING_LEN = 128;
    
    static EventStringPool& getInstance() {
        static EventStringPool instance;
        return instance;
    }
    
    // Register an event description, returns its index (0-8191).
    // If pool is full, overwrite the oldest entry (head position).
    // Copies up to MAX_STRING_LEN-1 chars, null-terminates.
    uint16_t registerEvent(const char* desc) {
        char* dest = pool_[head_];
        size_t len = desc ? std::strlen(desc) : 0;
        if (len > MAX_STRING_LEN - 1) len = MAX_STRING_LEN - 1;
        if (len > 0) std::memcpy(dest, desc, len);
        dest[len] = '\0';
        
        uint16_t index = head_;
        head_ = (head_ + 1) % MAX_EVENTS;
        if (count_ < MAX_EVENTS) count_++;
        return index;
    }
    
    const char* getEvent(uint16_t index) const {
        if (index >= MAX_EVENTS) return "";
        // Check if this slot has been written (first byte non-null)
        if (pool_[index][0] == '\0') return "";
        return pool_[index];
    }
    
    uint16_t getCount() const { return count_; }
    
    void clear() {
        head_ = 0; count_ = 0;
        for (size_t i = 0; i < MAX_EVENTS; i++) pool_[i][0] = '\0';
    }
    
private:
    EventStringPool() : head_(0), count_(0) {
        for (size_t i = 0; i < MAX_EVENTS; i++) pool_[i][0] = '\0';
    }
    
    char pool_[MAX_EVENTS][MAX_STRING_LEN];
    uint16_t head_;
    uint16_t count_;
};
