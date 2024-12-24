#pragma once

#include <unordered_map>
#include <vector>

#include "event.h"

class Thread {
   private:
    uint32_t thread_id_;

    Event prev_read_;

    std::vector<Event> events_;
    std::unordered_map<Event, Event, EventHash> event_to_prev_read_;

   public:
    Thread() = default;
    Thread(uint32_t thread_id) : thread_id_(thread_id), prev_read_(Event()) {}

    void addEvent(const Event e) {
        /* Assume events will be added in order of the original trace */
        events_.push_back(e);

        event_to_prev_read_[e] = prev_read_;

        if (e.getEventType() == Event::EventType::Read) {
            prev_read_ = e;
        }
    }

    const std::vector<Event>& getEvents() const { return events_; }

    const Event& getPrevRead(const Event& e) const {
        return event_to_prev_read_.at(e);
    }
};