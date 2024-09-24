#pragma once

#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

class Event {
private:
    // 4 bits event identifier, 8 bits thread identifier, 20 bits variable identifer, 32 bits variable value
    uint64_t raw_event;
    uint32_t event_id;
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

    Event() : raw_event(0), event_id(0) {}

    Event(uint64_t raw_event_, uint32_t event_id_) : raw_event(raw_event_), event_id(event_id_) {}

    EventType getEventType() const {
        return static_cast<EventType>((raw_event >> 60) & 0xF);
    }

    uint32_t getThreadId() const {
        return (raw_event >> 52) & 0xFF;
    } 

    uint32_t getVarId() const {
        return (raw_event >> 32) & 0xFFFFF;
    }

    uint32_t getVarValue() const {
        return raw_event & 0xFFFFFFFF;
    }

    uint32_t getEventId() const {
        return event_id;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "Event Type: " << getEventType() << ", "
            << "Thread ID: " << getThreadId() << ", "
            << "Variable ID: " << getVarId() << ", "
            << "Variable Value: " << getVarValue();
        return oss.str();
    }

    static uint64_t createRawEvent(EventType eventType, uint32_t threadId, uint32_t varId, uint32_t varValue) {
        eventType = static_cast<EventType>(eventType & 0xF);
        threadId = threadId & 0xFF;
        varId = varId & 0xFFFFF;
        varValue = varValue & 0xFFFFFFFF;

        return (static_cast<uint64_t>(eventType) << 60) |
               (static_cast<uint64_t>(threadId) << 52) |
               (static_cast<uint64_t>(varId) << 32) |
               varValue;
    }
};

std::ostream& operator<<(std::ostream& os, const Event& event) {
    return os << event.toString();
}