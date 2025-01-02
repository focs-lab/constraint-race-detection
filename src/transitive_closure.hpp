#include <unordered_map>
#include <utility>
#include <vector>

#include "event.hpp"

class TransitiveClosure {
   private:
    std::unordered_map<Event, uint32_t, EventHash> eventToGroup_;
    std::vector<std::vector<bool>> happens_before_;

    TransitiveClosure(
        std::unordered_map<Event, uint32_t, EventHash> eventToGroup,
        std::vector<std::vector<bool>> happens_before)
        : eventToGroup_(eventToGroup), happens_before_(happens_before) {}

   public:
    TransitiveClosure() = default;
    TransitiveClosure(TransitiveClosure&& other) noexcept
        : eventToGroup_(std::move(other.eventToGroup_)),
          happens_before_(std::move(other.happens_before_)) {}

    TransitiveClosure& operator=(TransitiveClosure&& other) noexcept {
        if (this != &other) {
            eventToGroup_ = std::move(other.eventToGroup_);
            happens_before_ = std::move(other.happens_before_);
        }
        return *this;
    }

    bool happensBefore(const Event& e1, const Event& e2) const {
        assert(eventToGroup_.find(e1) != eventToGroup_.end());
        assert(eventToGroup_.find(e2) != eventToGroup_.end());

        uint32_t group1 = eventToGroup_.at(e1);
        uint32_t group2 = eventToGroup_.at(e2);

        return happens_before_[group1][group2];
    }

    class Builder {
       private:
        std::unordered_map<Event, uint32_t, EventHash> eventToGroup_;
        std::vector<std::pair<Event, Event>> relations_;

        uint32_t groupCount_;

       public:
        Builder(int size) : groupCount_(0) { eventToGroup_.reserve(size); }

        void createNewGroup(const Event& e) {
            eventToGroup_[e] = groupCount_++;
        }

        /**
         * Add e1 to the same group as e2
         */
        void addToGroup(const Event& e1, const Event& e2) {
            assert(eventToGroup_.find(e2) != eventToGroup_.end());
            eventToGroup_[e1] = eventToGroup_.at(e2);
        }

        void addRelation(const Event& e1, const Event& e2) {
            relations_.emplace_back(e1, e2);
        }

        TransitiveClosure build() {
            std::vector<std::vector<bool>> hb(
                groupCount_, std::vector<bool>(groupCount_, false));

            for (const auto& [e1, e2] : relations_) {
                uint32_t group1 = eventToGroup_.at(e1);
                uint32_t group2 = eventToGroup_.at(e2);
                hb[group1][group2] = true;
            }

            for (int k = 0; k < groupCount_; k++) {
                for (int i = 0; i < groupCount_; i++) {
                    for (int j = 0; j < groupCount_; j++) {
                        hb[i][j] = hb[i][j] || (hb[i][k] && hb[k][j]);
                    }
                }
            }

            return TransitiveClosure(eventToGroup_, hb);
        }
    };
};