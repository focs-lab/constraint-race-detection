#pragma once

#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "event.cpp"

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

    Trace(
        std::vector<Event> raw_events_,
        std::unordered_map<uint32_t, std::vector<Event>> thread_to_events_map_,
        std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
            lock_pairs_,
        std::unordered_map<uint32_t, std::vector<Event>> reads_,
        std::unordered_map<uint32_t, std::vector<Event>> writes_,
        std::vector<std::pair<uint32_t, uint32_t>> fork_begin_pairs_,
        std::vector<std::pair<uint32_t, uint32_t>> join_end_pairs_,
        std::vector<std::pair<Event, Event>> cops_)
        : raw_events(raw_events_),
          thread_to_events_map(thread_to_events_map_),
          lock_pairs(lock_pairs_),
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
                    case Event::EventType::Acquire:
                        last_acquire_event[e.getVarId()] = e;
                        break;
                    case Event::EventType::Release:
                        lock_pairs[e.getVarId()].push_back(
                            {last_acquire_event[e.getVarId()], e});
                        break;
                    case Event::EventType::Read:
                        reads[e.getVarId()].push_back(e);
                        break;
                    case Event::EventType::Write:
                        writes[e.getVarId()].push_back(e);
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
                std::cerr << "Error parsing line: " << e.what() << " - " << e.what()
                          << std::endl;
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

        // std::cout << "Raw events: " << std::endl;
        // for (const Event& e : raw_events) {
        //     std::cout << e.prettyString() << std::endl;
        // }

        // std::cout << "lock pairs: " << std::endl;
        // for (const auto& [lockId, acq_rel_pairs] : lock_pairs) {
        //     for (const auto& [acq, rel] : acq_rel_pairs) {
        //         std::cout << acq.prettyString() << " - " <<
        //         rel.prettyString()
        //                   << std::endl;
        //     }
        // }

        // std::cout << "fork begin pairs: " << std::endl;
        // for (const auto& [fork, begin] : fork_begin_pairs) {
        //     std::cout << fork << " - " << begin << std::endl;
        // }

        // std::cout << "join end pairs: " << std::endl;
        // for (const auto& [join, end] : join_end_pairs) {
        //     std::cout << join << " - " << end << std::endl;
        // }

        return new Trace(raw_events, thread_to_events_map, lock_pairs, reads,
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
};
