#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

/**
 * Event class represents an event in the trace.
 * It contains information about the event type, thread id, target id,
 * taget value (only if target is shared memory address) and event id. Event id
 * of 0 represents a null event which can be used for initialization.
 */
class Event {
   private:
    /*
     * 4 bits event identifier, 8 bits thread identifier, 20 bits target
     * identifer, 32 bits target value
     */
    uint64_t raw_event_;
    uint32_t event_id_;

   public:
    enum EventType {
        Read = 0,
        Write = 1,
        Acquire = 2,
        Release = 3,
        Begin = 4,
        End = 5,
        Fork = 6,
        Join = 7
    };

    Event() : raw_event_(0), event_id_(0) {}

    Event(uint64_t raw_event_, uint32_t event_id_)
        : raw_event_(raw_event_), event_id_(event_id_) {}

    inline EventType getEventType() const {
        return static_cast<EventType>((raw_event_ >> 60) & 0xF);
    }

    inline uint32_t getThreadId() const { return (raw_event_ >> 52) & 0xFF; }

    inline uint32_t getTargetId() const { return (raw_event_ >> 32) & 0xFFFFF; }

    inline uint32_t getTargetValue() const { return raw_event_ & 0xFFFFFFFF; }

    inline uint32_t getEventId() const { return event_id_; }

    static inline bool isNullEvent(const Event& e) {
        return e.getEventId() == 0;
    }

    static uint64_t createRawEvent(EventType event_type, uint32_t thread_id,
                                   uint32_t target_id, uint32_t target_value) {
        return (static_cast<uint64_t>(event_type) << 60) |
               (static_cast<uint64_t>(thread_id) << 52) |
               (static_cast<uint64_t>(target_id) << 32) |
               static_cast<uint64_t>(target_value);
    }

    bool operator==(const Event& other) const {
        return raw_event_ == other.raw_event_ && event_id_ == other.event_id_;
    }
};

struct EventHash {
    std::size_t operator()(const Event& e) const {
        return std::hash<uint64_t>{}(e.getEventId());
    }
};