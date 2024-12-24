#include <unordered_map>
#include <vector>

#include "event.h"
#include "trace.h"

class LocksetEngine {
   private:
    std::unordered_map<uint32_t,
                       std::unordered_map<uint32_t, std::vector<LockRegion>>>
        lock_id_to_thread_id_to_lock_region_;

   public:
    LocksetEngine(
        std::unordered_map<
            uint32_t, std::unordered_map<uint32_t, std::vector<LockRegion>>>
            lock_id_to_thread_id_to_lock_region)
        : lock_id_to_thread_id_to_lock_region_(
              lock_id_to_thread_id_to_lock_region) {}

    bool hasCommonLock(const Event& e1, const Event& e2) const {
        for (const auto& [lock_id, thread_id_to_lock_region] :
             lock_id_to_thread_id_to_lock_region_) {
            if (thread_id_to_lock_region.find(e1.getThreadId()) ==
                    thread_id_to_lock_region.end() ||
                thread_id_to_lock_region.find(e2.getThreadId()) ==
                    thread_id_to_lock_region.end())
                continue;

            bool e1_has_lock = false;
            bool e2_has_lock = false;

            for (const auto& lock_region :
                 thread_id_to_lock_region.at(e1.getThreadId())) {
                if (lock_region.containsEvent(e1)) {
                    e1_has_lock = true;
                    break;
                }
            }

            for (const auto& lock_region :
                 thread_id_to_lock_region.at(e2.getThreadId())) {
                if (lock_region.containsEvent(e2)) {
                    e2_has_lock = true;
                    break;
                }
            }

            if (e1_has_lock && e2_has_lock) return true;
        }
        return false;
    }
};
