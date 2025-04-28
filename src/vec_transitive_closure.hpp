#pragma once

#include <iostream>
#include <unordered_map>
#include <vector>

#include "event.hpp"
#include "trace.hpp"
#include "vector_clock.hpp"

class VecTransitiveClosure {
   private:
    // Stores vector clock for each event
    std::vector<VectorClock> vector_clocks_;

   public:
    VecTransitiveClosure(const Trace& trace) {
        const uint32_t threadCount = trace.getThreads().size();
        const std::vector<Event> events = trace.getAllEvents();

        // Initialize vector clocks array with a dummy clock at index 0
        vector_clocks_.push_back(VectorClock(threadCount));

        // Maps to maintain thread state
        std::unordered_map<TID, VectorClock>
            thread_clocks;  // Current clock for each thread
        std::unordered_map<TID, uint32_t>
            thread_ids;  // Mapping from TID to 1-based index
        std::unordered_map<uint32_t, VectorClock>
            fork_clocks;  // Clocks at fork points
        std::unordered_map<uint32_t, VectorClock>
            join_clocks;  // Clocks at join points
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, VectorClock>>
            writer_clocks;  // Clocks for unique writers: var_id -> value ->
                            // clock

        uint32_t next_thread_id = 1;

        for (const Event& event : events) {
            if (thread_ids.find(event.getThreadId()) == thread_ids.end()) {
                assert(next_thread_id <= threadCount && "Too many threads");
                thread_ids[event.getThreadId()] = next_thread_id++;
                thread_clocks[event.getThreadId()] = VectorClock(threadCount);
            }

            VectorClock current_clock = thread_clocks[event.getThreadId()];
            const uint32_t thread_id = thread_ids[event.getThreadId()];

            current_clock.incrementThread(thread_id);

            switch (event.getEventType()) {
                case Event::EventType::Begin:
                    if (fork_clocks.find(event.getThreadId()) !=
                        fork_clocks.end()) {
                        // Thread was forked - inherit parent's clock
                        current_clock.setHappensBefore(
                            fork_clocks[event.getThreadId()]);
                    }
                    break;

                case Event::EventType::Fork:
                    // Save current clock for forked thread
                    fork_clocks[event.getTargetId()] = current_clock;
                    break;

                case Event::EventType::Join:
                    assert(join_clocks.find(event.getTargetId()) !=
                               join_clocks.end() &&
                           "Join before thread end");
                    // Inherit joined thread's clock
                    current_clock.setHappensBefore(
                        join_clocks[event.getTargetId()]);
                    break;

                case Event::EventType::End:
                    // Save clock for future joins
                    join_clocks[event.getThreadId()] = current_clock;
                    break;

                case Event::EventType::Write:
                    if (trace.getVariable(event.getTargetId())
                            .isUniqueWriter(event)) {
                        // Save clock for future reads
                        writer_clocks[event.getTargetId()]
                                     [event.getTargetValue()] = current_clock;
                    }
                    break;

                case Event::EventType::Read:
                    if (trace.getVariable(event.getTargetId())
                            .hasUniqueWriter(event)) {
                        // Must find the writer's clock
                        assert(writer_clocks.find(event.getTargetId()) !=
                                   writer_clocks.end() &&
                               "No writers found for variable");
                        assert(writer_clocks[event.getTargetId()].find(
                                   event.getTargetValue()) !=
                                   writer_clocks[event.getTargetId()].end() &&
                               "No writer found for this value");
                        // Inherit writer's clock
                        current_clock.setHappensBefore(
                            writer_clocks[event.getTargetId()]
                                         [event.getTargetValue()]);
                    }
                    break;

                case Event::EventType::Acquire:
                case Event::EventType::Release:
                    // These events only follow program order
                    // No additional synchronization needed
                    break;
            }

            // Store updated clock
            thread_clocks[event.getThreadId()] = current_clock;
            vector_clocks_.push_back(current_clock);
        }
    }

    bool happensBefore(const Event& e1, const Event& e2) const {
        assert(
            e1.getEventId() < vector_clocks_.size() &&
            ("Invalid event ID: " + std::to_string(e1.getEventId())).c_str());
        assert(
            e2.getEventId() < vector_clocks_.size() &&
            ("Invalid event ID: " + std::to_string(e2.getEventId())).c_str());

        const VectorClock& vc1 = vector_clocks_[e1.getEventId()];
        const VectorClock& vc2 = vector_clocks_[e2.getEventId()];

        return vc1 < vc2;
    }
};