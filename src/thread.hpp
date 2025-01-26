#pragma once

#include <unordered_map>
#include <vector>

#include "event.hpp"

class Thread {
   private:
    uint32_t thread_id_;

    Event first_read_;
    Event prev_read_;
    Event prev_acq_;

    std::vector<Event> events_;
    std::unordered_map<Event, Event, EventHash> event_to_prev_read_;
    std::unordered_map<Event, Event, EventHash> event_to_prev_acq_;

   public:
    Thread() = default;
    Thread(uint32_t thread_id) : thread_id_(thread_id), first_read_(Event()), prev_read_(Event()), prev_acq_(Event()) {}

    void addEvent(const Event e) {
        /* Assume events will be added in order of the original trace */
        events_.push_back(e);

        event_to_prev_read_[e] = prev_read_;

        if (e.getEventType() == Event::EventType::Acquire) 
            prev_acq_ = e;
        if (e.getEventType() == Event::EventType::Release)
            prev_acq_ = Event();

        event_to_prev_acq_[e] = prev_acq_;

        if (e.getEventType() == Event::EventType::Read) {
            prev_read_ = e;

            if (Event::isNullEvent(first_read_))
                first_read_ = e;
        }
    }

    uint32_t getThreadId() const {
        return thread_id_;
    }

    const std::vector<Event>& getEvents() const { return events_; }

    const Event& getFirstRead() const {
        return first_read_;
    }

    const Event& getPrevRead(const Event& e) const {
        return event_to_prev_read_.at(e);
    }

    const Event& getPrevAcq(const Event& e) const {
        return event_to_prev_acq_.at(e);
    }
};