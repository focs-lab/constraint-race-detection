#pragma once

#include <cassert>
#include <unordered_map>
#include <utility>
#include <vector>

#include "event.h"
#include "lock_region.h"
#include "thread.h"
#include "variable.h"

class Trace {
   private:
    std::vector<Event> all_events_;

    std::vector<std::pair<Event, Event>> fork_begin_pairs_;
    std::vector<std::pair<Event, Event>> end_join_pairs_;

    std::unordered_map<uint32_t, Thread> thread_id_to_thread_;

    std::unordered_map<uint32_t, Variable> var_id_to_variable_;

    std::unordered_map<uint32_t, std::vector<LockRegion>>
        lock_id_to_lock_region_;

    Trace(std::vector<Event> all_events,
          std::vector<std::pair<Event, Event>> fork_begin_pairs,
          std::vector<std::pair<Event, Event>> end_join_pairs,
          std::unordered_map<uint32_t, Thread> thread_id_to_thread,
          std::unordered_map<uint32_t, Variable> var_id_to_variable,
          std::unordered_map<uint32_t, std::vector<LockRegion>>
              lock_id_to_lock_region)
        : all_events_(all_events),
          fork_begin_pairs_(fork_begin_pairs),
          end_join_pairs_(end_join_pairs),
          thread_id_to_thread_(thread_id_to_thread),
          var_id_to_variable_(var_id_to_variable),
          lock_id_to_lock_region_(lock_id_to_lock_region) {}

   public:
    static Trace createTrace(const std::vector<uint64_t>& raw_events);
    static Trace fromBinaryFile(const std::string& filename);
    static Trace fromTextFile(const std::string& filename);

    std::vector<Event> getAllEvents() const;
    std::vector<Event> getGoodWritesForRead(const Event& read) const;
    std::vector<Event> getBadWritesForRead(const Event& read) const;

    std::vector<std::pair<Event, Event>> getCOPs() const;
    std::vector<std::pair<Event, Event>> getForkBeginPairs() const;
    std::vector<std::pair<Event, Event>> getEndJoinPairs() const;

    std::vector<Thread> getThreads() const;

    std::unordered_map<uint32_t, std::vector<LockRegion>>
    getLockRegions() const;
    std::unordered_map<
        uint32_t, std::unordered_map<uint32_t, std::vector<LockRegion>>>
    getThreadIdToLockIdToLockRegions() const;

    Event getPrevReadInThread(const Event& e) const;
    Event getSameThreadSameVarPrevWrite(const Event& e) const;
    Event getPrevDiffReadInThread(const Event& e) const;

    bool hasSameInitialValue(const Event& e) const;
};