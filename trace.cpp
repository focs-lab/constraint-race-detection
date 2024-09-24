#pragma once

#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "event.cpp"

class Trace {
   private:
    std::vector<Event> raw_events;
    std::unordered_map<uint32_t, std::vector<uint32_t>> thread_to_events_map;
    std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>>
        lock_pairs;
    std::unordered_map<uint32_t, std::vector<Event>> reads;
    std::unordered_map<uint32_t, std::vector<Event>> writes;
    std::vector<std::pair<uint32_t, uint32_t>> fork_begin_pairs;
    std::vector<std::pair<uint32_t, uint32_t>> join_end_pairs;
    std::vector<std::pair<Event, Event>> cops;

    Trace(std::vector<Event> raw_events_,
          std::unordered_map<uint32_t, std::vector<uint32_t>>
              thread_to_events_map_,
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
        std::unordered_map<uint32_t, std::vector<uint32_t>>
            thread_to_events_map;
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

        std::string line;
        uint32_t event_no = 1;
        while (std::getline(inputFile, line)) {
            try {
                uint64_t rawEvent = std::stoull(line);
                Event e = Event(rawEvent, event_no++);

                raw_events.push_back(e);
                thread_to_events_map[e.getThreadId()].push_back(e.getEventId());

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
                std::cerr << "Error parsing line: " << line << " - " << e.what()
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

        return new Trace(raw_events, thread_to_events_map, lock_pairs, reads,
                         writes, fork_begin_pairs, join_end_pairs, cops);
    }

    std::vector<Event> getAllEvents() const { return raw_events; }

    std::unordered_map<uint32_t, std::vector<uint32_t>> getThreadToEventsMap()
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
};
