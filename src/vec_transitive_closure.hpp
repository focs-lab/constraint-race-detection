#include <iostream>
#include <unordered_map>
#include <vector>

#include "event.hpp"
#include "trace.hpp"
#include "vector_clock.hpp"

class VecTransitiveClosure {
   private:
    std::vector<VectorClock> vector_clocks_;

   public:
    VecTransitiveClosure(const Trace& trace) {
        vector_clocks_.push_back(
            VectorClock());  // To help index more easily with EID

        std::unordered_map<TID, VectorClock> tid_to_last_vc;
        std::unordered_map<TID, uint32_t> thread_map;
        std::unordered_map<uint32_t, VectorClock> fork_vc;
        std::unordered_map<uint32_t, VectorClock> end_vc;
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, VectorClock>>
            unique_writer_vc;

        std::vector<Event> ed = trace.getAllEvents();

        uint32_t threadCount = trace.getThreads().size();
        uint32_t tc = 1;

        for (int i = 0; i < ed.size(); ++i) {
            Event& e = ed[i];

            VectorClock vc(threadCount);
            if (tid_to_last_vc.find(e.getThreadId()) == tid_to_last_vc.end()) {
                thread_map[e.getThreadId()] = tc;
                tc++;
                if (e.getEventType() == Event::EventType::Begin &&
                    fork_vc.find(e.getThreadId()) != fork_vc.end()) {
                    vc = fork_vc.at(e.getThreadId());
                }
            } else {
                vc = tid_to_last_vc[e.getThreadId()];
            }

            vc.incrementThread(thread_map[e.getThreadId()]);

            if (e.getEventType() == Event::EventType::End) {
                // assert(end_vc.find(e.getThreadId()) != end_vc.end());
                end_vc[e.getThreadId()] = vc;
            }

            if (e.getEventType() == Event::EventType::Fork) {
                fork_vc[e.getTargetId()] = vc;
            }

            if (e.getEventType() == Event::EventType::Join) {
                assert(end_vc.find(e.getTargetId()) != end_vc.end());
                vc.setHappensBefore(end_vc[e.getTargetId()]);
            }

            if (e.getEventType() == Event::EventType::Write) {
                Variable v = trace.getVariable(e.getTargetId());

                if (v.isUniqueWriter(e)) {
                    unique_writer_vc[e.getTargetId()][e.getTargetValue()] = vc;
                }
            }

            if (e.getEventType() == Event::EventType::Read) {
                Variable v = trace.getVariable(e.getTargetId());

                if (v.hasUniqueWriter(e)) {
                    assert(unique_writer_vc.find(e.getTargetId()) !=
                           unique_writer_vc.end());
                    assert(unique_writer_vc[e.getTargetId()].find(
                               e.getTargetValue()) !=
                           unique_writer_vc[e.getTargetId()].end());
                    vc.setHappensBefore(
                        unique_writer_vc[e.getTargetId()][e.getTargetValue()]);
                }
            }

            tid_to_last_vc[e.getThreadId()] = vc;
            vector_clocks_.push_back(vc);
        }
    }

    bool happensBefore(const Event& e1, const Event& e2) const {
        assert(e1.getEventId() < vector_clocks_.size());
        assert(e2.getEventId() < vector_clocks_.size());

        return vector_clocks_[e1.getEventId()] <
               vector_clocks_[e2.getEventId()];
    }
};