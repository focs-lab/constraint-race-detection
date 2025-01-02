#pragma once

#include <unordered_map>
#include <vector>

#include "event.hpp"

class Variable {
   private:
    uint32_t var_id_;

    Event first_read_;
    Event first_write_;

    std::vector<Event> writes_;

    std::unordered_map<uint32_t, std::vector<Event>> var_val_to_write_events_;
    std::unordered_map<uint32_t, std::vector<Event>> tid_to_read_events_;
    std::unordered_map<uint32_t, std::vector<Event>> tid_to_write_events_;
    std::unordered_map<Event, Event, EventHash> read_to_prev_write_in_thread_;
    std::unordered_map<Event, Event, EventHash>
        read_to_prev_diff_read_in_thread_;

   public:
    Variable() = default;

    Variable(uint32_t var_id)
        : var_id_(var_id), first_read_(Event()), first_write_(Event()) {}

    void addEvent(const Event& e) {
        /* Assume events will be added in order of the original trace */
        if (e.getEventType() == Event::EventType::Read) {
            if (Event::isNullEvent(first_read_) &&
                Event::isNullEvent(first_write_))
                first_read_ = e;

            read_to_prev_diff_read_in_thread_[e] = Event();

            if (tid_to_read_events_.find(e.getThreadId()) !=
                tid_to_read_events_.end()) {
                for (auto it = tid_to_read_events_[e.getThreadId()].rbegin();
                     it != tid_to_read_events_[e.getThreadId()].rend(); ++it) {
                    if (it->getTargetValue() != e.getTargetValue()) {
                        read_to_prev_diff_read_in_thread_[e] = *it;
                        break;
                    }
                }
            }

            tid_to_read_events_[e.getThreadId()].push_back(e);

            if (tid_to_write_events_.find(e.getThreadId()) !=
                tid_to_write_events_.end()) {
                read_to_prev_write_in_thread_[e] =
                    tid_to_write_events_.at(e.getThreadId()).back();
            }
        } else if (e.getEventType() == Event::EventType::Write) {
            writes_.push_back(e);

            if (Event::isNullEvent(first_write_) &&
                Event::isNullEvent(first_read_))
                first_write_ = e;

            tid_to_write_events_[e.getThreadId()].push_back(e);
            var_val_to_write_events_[e.getTargetValue()].push_back(e);
        }
    }

    bool sameInitialValue(const Event& read) const {
        assert(read.getEventType() == Event::EventType::Read);

        if (!Event::isNullEvent(first_write_)) return false;

        return read.getTargetValue() == first_read_.getTargetValue();
    };

    const std::vector<Event> getGoodWrites(const Event& read) const {
        assert(read.getEventType() == Event::EventType::Read);

        if (var_val_to_write_events_.find(read.getTargetValue()) ==
            var_val_to_write_events_.end()) {
            std::vector<Event> emptyVector;

            return emptyVector;  // possible for a read to have no good
                                 // writes
        }

        return var_val_to_write_events_.at(read.getTargetValue());
    };
    const std::vector<Event> getBadWrites(const Event& read) const {
        assert(read.getEventType() == Event::EventType::Read);

        std::vector<Event> bad_writes;
        for (const auto& [val, writes] : var_val_to_write_events_) {
            if (read.getTargetValue() != val) {
                bad_writes.insert(bad_writes.end(), writes.begin(),
                                  writes.end());
            }
        }

        return bad_writes;
    };

    const Event getPrevWriteInThread(const Event& e) const {
        assert(e.getEventType() == Event::EventType::Read);

        if (read_to_prev_write_in_thread_.find(e) ==
            read_to_prev_write_in_thread_.end()) {
            Event emptyEvent;
            return emptyEvent;  // no prev write so return null event
        }

        return read_to_prev_write_in_thread_.at(e);
    }

    const Event getPrevDiffReadInThread(const Event& e) const {
        assert(e.getEventType() == Event::EventType::Read);

        if (read_to_prev_diff_read_in_thread_.find(e) ==
            read_to_prev_diff_read_in_thread_.end()) {
            Event emptyEvent;
            return emptyEvent;  // no prev diff read so return null event
        }

        return read_to_prev_diff_read_in_thread_.at(e);
    }

    std::vector<std::pair<Event, Event>> getCOP() const {
        std::vector<std::pair<Event, Event>> cop;

        for (int i = 0; i < writes_.size(); ++i) {
            for (int j = i + 1; j < writes_.size(); ++j) {
                if (writes_[i].getThreadId() == writes_[j].getThreadId())
                    continue;
                cop.emplace_back(writes_[i], writes_[j]);
            }
            for (const auto& [rtid, reads] : tid_to_read_events_) {
                if (writes_[i].getThreadId() == rtid) continue;
                for (const Event& read : reads) {
                    cop.emplace_back(writes_[i], read);
                }
            }
        }

        return cop;
    }
};