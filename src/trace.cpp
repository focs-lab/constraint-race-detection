#pragma once

#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "event.cpp"

class LockRegion {
   private:
    uint32_t lockId;
    uint32_t acqEvent;
    uint32_t relEvent;
    uint32_t threadId;

   public:
    LockRegion(uint32_t lockId, uint32_t acqEvent, uint32_t relEvent,
               uint32_t threadId)
        : lockId(lockId),
          acqEvent(acqEvent),
          relEvent(relEvent),
          threadId(threadId) {}

    uint32_t getAcqEventId() const { return acqEvent; }

    uint32_t getRelEventId() const { return relEvent; }

    bool containsEvent(const Event& e) const {
        return e.getThreadId() == threadId && e.getEventId() >= acqEvent &&
               e.getEventId() <= relEvent;
    }
};

class Trace {
   private:
    std::vector<Event> raw_events;
    std::unordered_map<uint32_t, std::vector<Event>> thread_to_events_map;
    std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
        lock_pairs;
    std::unordered_map<uint32_t, std::vector<Event>> reads;
    std::unordered_map<uint32_t, std::vector<Event>> writes;
    std::vector<std::pair<uint32_t, uint32_t>> fork_begin_pairs;
    std::vector<std::pair<uint32_t, uint32_t>> join_end_pairs;
    std::vector<std::pair<Event, Event>> cops;

    std::unordered_map<uint32_t,
                       std::unordered_map<uint32_t, std::vector<LockRegion>>>
        lock_regions;

    std::unordered_map<uint32_t, std::unordered_map<uint32_t, Event>>
        thread_to_event_to_last_read;

    std::unordered_map<uint32_t, std::vector<Event>> reads_to_feasible_writes;
    std::unordered_map<uint32_t, std::vector<Event>> reads_to_infeasible_writes;
    std::unordered_map<uint32_t, Event> same_thread_same_var_prev_write;

    Trace(
        std::vector<Event> raw_events_,
        std::unordered_map<uint32_t, std::vector<Event>> thread_to_events_map_,
        std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
            lock_pairs_,
        std::unordered_map<
            uint32_t, std::unordered_map<uint32_t, std::vector<LockRegion>>>
            lock_regions_,
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, Event>>
            thread_to_event_to_last_read_,
        std::unordered_map<uint32_t, std::vector<Event>> reads_,
        std::unordered_map<uint32_t, std::vector<Event>> writes_,
        std::vector<std::pair<uint32_t, uint32_t>> fork_begin_pairs_,
        std::vector<std::pair<uint32_t, uint32_t>> join_end_pairs_,
        std::vector<std::pair<Event, Event>> cops_)
        : raw_events(raw_events_),
          thread_to_events_map(thread_to_events_map_),
          lock_pairs(lock_pairs_),
          lock_regions(lock_regions_),
          thread_to_event_to_last_read(thread_to_event_to_last_read_),
          reads(reads_),
          writes(writes_),
          fork_begin_pairs(fork_begin_pairs_),
          join_end_pairs(join_end_pairs_),
          cops(cops_) {
            generateFeasibleWrites();
          }

    void generateFeasibleWrites() {
        for (const auto& [varId, readVec] : reads) {
            for (const Event& r : readVec) {
                for (const Event& e : getWriteEvents(varId)) {
                    if (r.getThreadId() == e.getThreadId() && e.getEventId() > r.getEventId())
                        continue;
                    
                    if (r.getThreadId() == e.getThreadId()) {
                        same_thread_same_var_prev_write[r.getEventId()] = e;
                        continue;
                    }

                    if (e.getVarValue() == r.getVarValue()) {
                        reads_to_feasible_writes[r.getEventId()].push_back(e);
                    } else {
                        reads_to_infeasible_writes[r.getEventId()].push_back(e);
                    }
                }
            }
        }
    }

   public:
    static Trace* fromLog(const std::string& filename) {
        std::ifstream inputFile(filename);
        std::vector<Event> raw_events;
        std::unordered_map<uint32_t, std::vector<Event>> thread_to_events_map;
        std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
            lock_pairs;
        std::unordered_map<uint32_t, Event> last_acquire_event;
        std::unordered_map<uint32_t, std::vector<Event>> reads;
        std::unordered_map<uint32_t, std::vector<Event>> writes;
        std::unordered_map<uint32_t, uint32_t> forks;
        std::unordered_map<uint32_t, uint32_t> ends;
        std::vector<std::pair<uint32_t, uint32_t>> fork_begin_pairs;
        std::vector<std::pair<uint32_t, uint32_t>> join_end_pairs;
        std::vector<std::pair<Event, Event>> cops;

        std::unordered_map<
            uint32_t, std::unordered_map<uint32_t, std::vector<LockRegion>>>
            lock_regions;
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, Event>>
            lockId_to_threadId_to_last_acquire_event;

        std::unordered_map<uint32_t, Event> prev_read_in_thread;
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, Event>>
            thread_to_event_to_last_read;

        if (!inputFile.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return nullptr;
        }

        uint64_t rawEvent;

        /* We will start event IDs from 1, this way we can use event ID 0 to
         * represent an null event */
        uint32_t event_no = 1;
        while (inputFile.read(reinterpret_cast<char*>(&rawEvent),
                              sizeof(rawEvent))) {
            try {
                Event e = Event(rawEvent, event_no++);

                raw_events.push_back(e);
                thread_to_events_map[e.getThreadId()].push_back(e);

                switch (e.getEventType()) {
                    case Event::EventType::Acquire: {
                        last_acquire_event[e.getVarId()] = e;
                        lockId_to_threadId_to_last_acquire_event
                            [e.getVarId()][e.getThreadId()] = e;
                        break;
                    }
                    case Event::EventType::Release: {
                        lock_pairs[e.getVarId()].push_back(
                            {last_acquire_event[e.getVarId()], e});
                        Event last_acquire =
                            lockId_to_threadId_to_last_acquire_event
                                [e.getVarId()][e.getThreadId()];
                        lock_regions[e.getVarId()][e.getThreadId()].push_back(
                            LockRegion(e.getVarId(), last_acquire.getEventId(),
                                       e.getEventId(), e.getThreadId()));
                        break;
                    }
                    case Event::EventType::Read:
                        reads[e.getVarId()].push_back(e);
                        if (prev_read_in_thread.find(e.getThreadId()) !=
                            prev_read_in_thread.end()) {
                            thread_to_event_to_last_read
                                [e.getThreadId()][e.getEventId()] =
                                    prev_read_in_thread[e.getThreadId()];
                        } else {
                            thread_to_event_to_last_read[e.getThreadId()]
                                                        [e.getEventId()] =
                                                            Event();
                        }
                        prev_read_in_thread[e.getThreadId()] = e;
                        break;
                    case Event::EventType::Write:
                        writes[e.getVarId()].push_back(e);
                        if (prev_read_in_thread.find(e.getThreadId()) !=
                            prev_read_in_thread.end()) {
                            thread_to_event_to_last_read
                                [e.getThreadId()][e.getEventId()] =
                                    prev_read_in_thread[e.getThreadId()];
                        } else {
                            thread_to_event_to_last_read[e.getThreadId()]
                                                        [e.getEventId()] =
                                                            Event();
                        }
                        break;
                    case Event::EventType::Fork:
                        forks[e.getVarId()] = e.getEventId();
                        break;
                    case Event::EventType::Begin:
                        fork_begin_pairs.push_back(
                            {forks[e.getThreadId()], e.getEventId()});
                        break;
                    case Event::EventType::End:
                        ends[e.getThreadId()] = e.getEventId();
                        break;
                    case Event::EventType::Join:
                        join_end_pairs.push_back(
                            {ends[e.getVarId()], e.getEventId()});
                        break;
                    default:
                        break;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing line: " << e.what() << " - "
                          << e.what() << std::endl;
                return nullptr;
            }
        }

        inputFile.close();

        for (const auto& [varId, writeEvents] : writes) {
            for (int i = 0; i < writeEvents.size(); ++i) {
                Event writeEvent = writeEvents[i];
                for (int j = i + 1; j < writeEvents.size(); ++j) {
                    Event writeEvent2 = writeEvents[j];
                    if (writeEvent.getThreadId() != writeEvent2.getThreadId())
                        cops.push_back({writeEvent, writeEvent2});
                }
                for (int j = 0; j < reads[varId].size(); ++j) {
                    Event readEvent = reads[varId][j];
                    if (readEvent.getThreadId() != writeEvent.getThreadId())
                        cops.push_back({writeEvent, readEvent});
                }
            }
        }

        return new Trace(raw_events, thread_to_events_map, lock_pairs,
                         lock_regions, thread_to_event_to_last_read, reads,
                         writes, fork_begin_pairs, join_end_pairs, cops);
    }

    std::vector<Event> getAllEvents() const { return raw_events; }

    std::unordered_map<uint32_t, std::vector<Event>> getThreadToEventsMap()
        const {
        return thread_to_events_map;
    }

    std::vector<std::pair<uint32_t, uint32_t>> getForkBeginPairs() const {
        return fork_begin_pairs;
    }

    std::vector<std::pair<uint32_t, uint32_t>> getJoinEndPairs() const {
        return join_end_pairs;
    }

    std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
    getLockPairs() const {
        return lock_pairs;
    }

    std::vector<std::pair<Event, Event>> getCOPEvents() const { return cops; }

    std::vector<Event> getWriteEvents(uint32_t varId) const {
        if (writes.find(varId) == writes.end()) return {};
        return writes.at(varId);
    }

    std::vector<Event> getReadEvents(uint32_t varId) const {
        if (reads.find(varId) == reads.end()) return {};
        return reads.at(varId);
    }

    std::vector<Event> getEventsFromThread(uint32_t threadId) const {
        if (thread_to_events_map.find(threadId) == thread_to_events_map.end())
            return {};
        return thread_to_events_map.at(threadId);
    }

    std::unordered_map<uint32_t,
                       std::unordered_map<uint32_t, std::vector<LockRegion>>>
    getLockRegions() const {
        return lock_regions;
    }

    Event getPrevReadInThread(Event e) const {
        auto thread_it = thread_to_event_to_last_read.find(e.getThreadId());
        if (thread_it == thread_to_event_to_last_read.end()) {
            return Event();
        }

        auto event_it = thread_it->second.find(e.getEventId());
        if (event_it == thread_it->second.end()) {
            return Event();
        }

        return event_it->second;
    }

    std::vector<Event> getFeasibleWrites(Event& r) const {
        if (reads_to_feasible_writes.find(r.getEventId()) == reads_to_feasible_writes.end()) 
            return {};
        return reads_to_feasible_writes.at(r.getEventId());
    } 

    std::vector<Event> getInFeasibleWrites(Event& r) const {
        if (reads_to_infeasible_writes.find(r.getEventId()) == reads_to_infeasible_writes.end()) 
            return {};
        return reads_to_infeasible_writes.at(r.getEventId());
    } 

    Event getSameThreadSameVarPrevWrite(Event& r) const {
        if (same_thread_same_var_prev_write.find(r.getEventId()) == same_thread_same_var_prev_write.end()) 
            return Event();
        return same_thread_same_var_prev_write.at(r.getEventId());
    }
};
