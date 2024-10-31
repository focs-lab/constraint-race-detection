#pragma once
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

class TransitiveClosure {
   private:
    std::unordered_map<uint32_t, uint32_t> eventIdToGroupId;
    std::vector<std::vector<bool>> relation;

    TransitiveClosure(std::unordered_map<uint32_t, uint32_t>& eventIdToGroupId,
                      std::vector<std::vector<bool>>& relation)
        : eventIdToGroupId(eventIdToGroupId), relation(relation) {}

   public:
    TransitiveClosure() = default;
    TransitiveClosure(TransitiveClosure&& other) noexcept
        : eventIdToGroupId(std::move(other.eventIdToGroupId)),
          relation(std::move(other.relation)) {}

    // Move assignment operator
    TransitiveClosure& operator=(TransitiveClosure&& other) noexcept {
        if (this != &other) {
            eventIdToGroupId = std::move(other.eventIdToGroupId);
            relation = std::move(other.relation);
        }
        return *this;
    }
    class Builder {
       private:
        std::unordered_map<uint32_t, uint32_t> eventIdToGroupId;
        std::vector<std::pair<uint32_t, uint32_t>> relations;

        uint32_t groupCountSize;

       public:
        Builder(int size) : groupCountSize(0) {
            eventIdToGroupId.reserve(size);
        }

        void createNewGroup(uint32_t e) {
            eventIdToGroupId[e] = groupCountSize++;
        }

        /**
         * Add e1 to the same group as e2
         */
        void addToGroup(uint32_t e1, uint32_t e2) {
            eventIdToGroupId[e1] = eventIdToGroupId[e2];
        }

        void addRelation(uint32_t e1, uint32_t e2) {
            relations.emplace_back(e1, e2);
        }

        TransitiveClosure build() {
            uint32_t numOfGroups = groupCountSize;

            std::vector<std::vector<bool>> f(
                numOfGroups, std::vector<bool>(numOfGroups, false));

            for (const auto& p : relations) {
                uint32_t xGroup = eventIdToGroupId[p.first];
                uint32_t yGroup = eventIdToGroupId[p.second];
                f[xGroup][yGroup] = true;
            }

            for (int k = 0; k < numOfGroups; ++k) {
                for (int i = 0; i < numOfGroups; ++i) {
                    for (int j = 0; j < numOfGroups; ++j) {
                        f[i][j] = f[i][j] || (f[i][k] && f[k][j]);
                    }
                }
            }

            return TransitiveClosure(eventIdToGroupId, f);
        }
    };
    bool happensBefore(uint32_t e1, uint32_t e2) {
        return relation[eventIdToGroupId[e1]][eventIdToGroupId[e2]];
    }

    void print() {
        std::cout << "EventId to GroupId" << std::endl;
        for (const auto& [k, v] : eventIdToGroupId) {
            std::cout << k << " " << v << std::endl;
        }
        std::cout << "Relation" << std::endl;
        for (const auto& row : relation) {
            for (bool b : row) {
                std::cout << b << " ";
            }
            std::cout << std::endl;
        }
    }

    static class Builder builder(int size) { return Builder(size); }
};
