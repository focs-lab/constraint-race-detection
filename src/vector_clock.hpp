#include <cassert>
#include <vector>

typedef uint32_t timestamp;

class VectorClock {
   private:
    std::vector<timestamp> clock_;

   public:
    VectorClock() {}

    VectorClock(uint32_t thread_count) : clock_(thread_count + 1, 0) {}

    VectorClock(const VectorClock& vc) {
        clock_ = vc.clock_;
    }

    void incrementThread(uint32_t tid) {
        assert(tid > 0);
        assert(tid <= clock_.size());
        clock_[tid] += 1;
    }

    void setHappensBefore(const VectorClock& prev) {
        for (size_t i = 0; i < clock_.size(); ++i) {
            clock_[i] = std::max(clock_[i], prev.clock_[i]);
        }
    }

    bool operator<(const VectorClock& other) const {
        assert(clock_.size() == other.clock_.size());

        for (size_t i = 0; i < clock_.size(); ++i) {
            if (clock_[i] > other.clock_[i]) return false;
        }

        return true;
    }

    void print() {
        for (auto t : clock_) {
            std::cout << t << ", ";
        }
        std::cout << "\n";
    }
};