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

    Trace(
        std::vector<Event> raw_events_,
        std::unordered_map<uint32_t, std::vector<Event>> thread_to_events_map_,
        std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
            lock_pairs_,
        std::unordered_map<
            uint32_t, std::unordered_map<uint32_t, std::vector<LockRegion>>>
            lock_regions_,
        std::unordered_map<uint32_t, std::vector<Event>> reads_,
        std::unordered_map<uint32_t, std::vector<Event>> writes_,
        std::vector<std::pair<uint32_t, uint32_t>> fork_begin_pairs_,
        std::vector<std::pair<uint32_t, uint32_t>> join_end_pairs_,
        std::vector<std::pair<Event, Event>> cops_)
        : raw_events(raw_events_),
          thread_to_events_map(thread_to_events_map_),
          lock_pairs(lock_pairs_),
          lock_regions(lock_regions_),
          reads(reads_),
          writes(writes_),
          fork_begin_pairs(fork_begin_pairs_),
          join_end_pairs(join_end_pairs_),
          cops(cops_) {}

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

        std::unordered_map<uint32_t, uint32_t> prev_read_in_thread;
        std::unordered_map<uint32_t, uint32_t> thread_to_last_read;

        if (!inputFile.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return nullptr;
        }

        uint64_t rawEvent;
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
                        thread_to_last_read[e.getThreadId()] = e.getEventId();
                        break;
                    case Event::EventType::Write:
                        writes[e.getVarId()].push_back(e);
                        if (thread_to_last_read.find(e.getThreadId()) !=
                            thread_to_last_read.end()) {
                            prev_read_in_thread[e.getThreadId()] =
                                thread_to_last_read[e.getThreadId()];
                        } else {
                            prev_read_in_thread[e.getThreadId()] = 0;
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

        return new Trace(raw_events, thread_to_events_map, lock_pairs, lock_regions, reads,
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

    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::vector<LockRegion>>> getLockRegions() const {
        return lock_regions;
    }
};
