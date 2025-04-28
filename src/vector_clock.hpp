#pragma once

#include <cassert>
#include <iostream>
#include <vector>

typedef uint32_t timestamp;

class VectorClock {
   private:
    std::vector<timestamp> clock_;

   public:
    // Default constructor for empty vector clock
    VectorClock() {}

    // Initialize vector clock with thread_count threads (1-based indexing)
    VectorClock(uint32_t thread_count) : clock_(thread_count + 1, 0) {}

    // Copy constructor
    VectorClock(const VectorClock& other) : clock_(other.clock_) {}

    // Get the timestamp for a specific thread
    timestamp getTime(uint32_t tid) const {
        assert(tid > 0 && tid < clock_.size() && "Invalid thread ID");
        return clock_[tid];
    }

    // Increment logical clock for a specific thread
    void incrementThread(uint32_t tid) {
        assert(tid > 0 && tid < clock_.size() && "Invalid thread ID");
        clock_[tid]++;
    }

    // Update vector clock to reflect happens-before relationship with prev
    void setHappensBefore(const VectorClock& prev) {
        assert(clock_.size() == prev.clock_.size() &&
               "Vector clock size mismatch");
        for (size_t i = 0; i < clock_.size(); ++i) {
            clock_[i] = std::max(clock_[i], prev.clock_[i]);
        }
    }

    // Returns true if this vector clock happens before other
    // Implements the happens-before relation for vector clocks:
    // VC1 < VC2 iff:
    // 1) For all i, VC1[i] <= VC2[i]
    // 2) There exists at least one j where VC1[j] < VC2[j]
    bool operator<(const VectorClock& other) const {
        assert(clock_.size() == other.clock_.size() &&
               "Vector clock size mismatch");

        bool strictly_less = false;
        for (size_t i = 0; i < clock_.size(); ++i) {
            if (clock_[i] > other.clock_[i]) {
                return false;  // Found a position where this is greater
            }
            if (clock_[i] < other.clock_[i]) {
                strictly_less =
                    true;  // Found at least one strictly less position
            }
        }
        return strictly_less;  // True only if all <= and at least one <
    }

    // For debugging
    void print() const {
        std::cout << "[";
        for (size_t i = 1; i < clock_.size(); ++i) {
            if (i > 1) std::cout << ", ";
            std::cout << clock_[i];
        }
        std::cout << "]";
    }

    // Get size for debugging
    size_t size() const { return clock_.size(); }
};