#include <z3++.h>

#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
#include <chrono>

#include "event.cpp"
#include "maximal_casual_model.cpp"
#include "z3_maximal_casual_model.cpp"
#include "trace.cpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Please provide an input file\n";
        return 0;
    }

    std::string filename = argv[1];

    uint32_t maxNoOfCOP = 0;
    if (argc == 3) {
        try {
            maxNoOfCOP = std::stoul(argv[2]);
        } catch (std::exception& e) {
            std::cerr << "Invalid max number of COP events\n";
            return 1;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    Trace* trace = Trace::fromLog(filename);
    if (trace == nullptr) {
        std::cerr << "Error parsing log file" << std::endl;
        return 1;
    }

    Z3MaximalCasualModel z3mcm(*trace);

    uint32_t race_count;
    if (maxNoOfCOP) {
        race_count = z3mcm.solveForRace(maxNoOfCOP);
    } else {
        race_count = z3mcm.solveForRace();
    }
    
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Number of races detected: " << race_count << std::endl;
    std::cout << "Total time taken: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    return 0;
}