#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdexcept>

struct Arguments {
    std::string executionTrace;  // -f compulsory
    bool logWitness = false;     // --log_witness optional, default false
    uint32_t maxNoOfCOP = 0;     // -c optional

    static Arguments fromArgs(int argc, char* argv[]) {
        std::string executionTrace;
        bool logWitness = false;
        uint32_t maxNoOfCOP = 0;

        std::vector<std::string> arguments(argv + 1, argv + argc);

        auto itr = std::find(arguments.begin(), arguments.end(), "-f");
        if (itr != arguments.end() && itr + 1 != arguments.end()) {
            executionTrace = *(++itr);
        } else {
            throw std::runtime_error("Please provide an input file");
        }

        itr = std::find(arguments.begin(), arguments.end(), "-c");
        if (itr != arguments.end() && itr + 1 != arguments.end()) {
            try {
                maxNoOfCOP = std::stoul(*(++itr));
            } catch (std::exception& e) {
                throw std::runtime_error("Invalid max number of COP events");
            }
        }

        logWitness = std::find(arguments.begin(), arguments.end(),
                               "--log_witness") != arguments.end();

        return {executionTrace, logWitness, maxNoOfCOP};
    }
};