#pragma once

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

class Event {
   private:
    // 4 bits event identifier, 8 bits thread identifier, 20 bits variable
    // identifer, 32 bits variable value
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

    Event(uint64_t raw_event_, uint32_t event_id_)
        : raw_event(raw_event_), event_id(event_id_) {}

    EventType getEventType() const {
        return static_cast<EventType>((raw_event >> 60) & 0xF);
    }

    uint32_t getThreadId() const { return (raw_event >> 52) & 0xFF; }

    uint32_t getVarId() const { return (raw_event >> 32) & 0xFFFFF; }

    uint32_t getVarValue() const { return raw_event & 0xFFFFFFFF; }

    uint32_t getEventId() const { return event_id; }

    std::string toString() const {
        std::ostringstream oss;
        oss << "Event Id: " << getEventId();
        return oss.str();
    }

    std::string prettyString() const {
        std::ostringstream oss;
        std::string event_type;
        std::string var_prefix;
        switch (getEventType()) {
            case EventType::Read:
                event_type = "Read";
                var_prefix = "X_";
                break;
            case EventType::Write:
                event_type = "Write";
                var_prefix = "X_";
                break;
            case EventType::Acquire:
                event_type = "Acq";
                var_prefix = "l_";
                break;
            case EventType::Release:
                event_type = "Rel";
                var_prefix = "l_";
                break;
            case EventType::Begin:
                event_type = "Begin";
                var_prefix = "";
                break;
            case EventType::End:
                event_type = "End";
                var_prefix = "";
                break;
            case EventType::Fork:
                event_type = "Fork";
                var_prefix = "T_";
                break;
            case EventType::Join:
                event_type = "Join";
                var_prefix = "T_";
                break;
        }
        oss << event_type << " " << getThreadId() << " " << var_prefix
            << getVarId() << " " << getVarValue();
        return oss.str();
    }

    static uint64_t createRawEvent(EventType eventType, uint32_t threadId,
                                   uint32_t varId, uint32_t varValue) {
        eventType = static_cast<EventType>(eventType & 0xF);
        threadId = threadId & 0xFF;
        varId = varId & 0xFFFFF;
        varValue = varValue & 0xFFFFFFFF;

        return (static_cast<uint64_t>(eventType) << 60) |
               (static_cast<uint64_t>(threadId) << 52) |
               (static_cast<uint64_t>(varId) << 32) | varValue;
    }

    static bool isNullEvent(Event e) {
        return e.getEventId() == 0;
    }
};

std::ostream& operator<<(std::ostream& os, const Event& event) {
    return os << event.toString();
}