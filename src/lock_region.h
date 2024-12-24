#pragma once

#include <cassert>

#include "event.h"

/**
 * LockRegion class represents a lock region in the trace.
 * A lock region is the region within a thread where by events are protected by
 * the lock.
 */
class LockRegion {
   private:
    Event acq_event_;
    Event rel_event_;

   public:
    LockRegion(Event acq_event, Event rel_event)
        : acq_event_(acq_event), rel_event_(rel_event) {}

    Event getAcqEvent() const { return acq_event_; }

    Event getRelEvent() const { return rel_event_; }

    uint32_t getRegionThreadId() const { return acq_event_.getThreadId(); }

    bool containsEvent(const Event& e) const {
        assert(acq_event_.getEventId() < rel_event_.getEventId() &&
               acq_event_.getThreadId() == rel_event_.getThreadId());

        return e.getThreadId() == acq_event_.getThreadId() &&
               e.getEventId() >= acq_event_.getEventId() &&
               e.getEventId() <= rel_event_.getEventId();
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "LockRegion: " << acq_event_.getEventId() << " - "
           << rel_event_.getEventId();
        return ss.str();
    }
};