#pragma once
#include <cstdint>

class Model {
   public:
    virtual uint32_t solveForRace() = 0;
    virtual uint32_t solveForRace(uint32_t maxNoOfCOP) = 0;
    virtual void getStatistics() = 0;
};