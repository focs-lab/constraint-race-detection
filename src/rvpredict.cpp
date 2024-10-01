#include <z3++.h>

#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
#include <chrono>

#include "event.cpp"
#include "maximal_casual_model.cpp"
#include "trace.cpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Please provide an input file\n";
        return 0;
    }

    std::string filename = argv[1];

    auto start = std::chrono::high_resolution_clock::now();

    Trace* trace = Trace::fromLog(filename);
    if (trace == nullptr) {
        std::cerr << "Error parsing log file" << std::endl;
        return 1;
    }

    MaximalCasualModel mcm(*trace);

    mcm.generateMHBConstraints();
    #ifdef DEBUG
    std::cout << "Generated MHB constraints" << std::endl;
    #endif

    mcm.generateLockConstraints();
    #ifdef DEBUG
    std::cout << "Generated lock constraints" << std::endl;
    #endif

    mcm.generateReadCFConstraints();
    #ifdef DEBUG
    std::cout << "Generated CF constraints" << std::endl;
    std::cout << std::endl;
    #endif
    
    uint32_t race_count = mcm.solveForRace();

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Number of races detected: " << race_count << std::endl;
    std::cout << "Time taken: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    return 0;
}