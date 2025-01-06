#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

struct Arguments {
    std::string executionTrace;  // -f compulsory
    std::string witnessDir = "witness/"; // --witness-dir optional, default "witness/"
    bool logWitness = false;     // --log-witness optional, default false
    bool logBinaryWitness = false; // --log-binary-witness optional, default false
    bool binaryFormat = true;    // --human optional, default true
    uint32_t maxNoOfCOP = 0;     // -c optional
    uint32_t maxNoOfRace = 0;    // -r optional

    static Arguments fromArgs(int argc, char* argv[]) {
        std::string executionTrace;
        std::string witnessDir = "witness/";
        bool logWitness = false;
        bool logBinaryWitness = false;
        bool binaryFormat = true;
        uint32_t maxNoOfCOP = 0;
        uint32_t maxNoOfRace = 0;

        std::vector<std::string> arguments(argv + 1, argv + argc);

        auto itr = std::find(arguments.begin(), arguments.end(), "-f");
        if (itr != arguments.end() && itr + 1 != arguments.end()) {
            executionTrace = *(++itr);
        } else {
            throw std::runtime_error("Please provide an input file");
        }

        itr = std::find(arguments.begin(), arguments.end(), "--witness-dir");
        if (itr != arguments.end() && itr + 1 != arguments.end()) {
            witnessDir = *(++itr);
        }

        itr = std::find(arguments.begin(), arguments.end(), "-c");
        if (itr != arguments.end() && itr + 1 != arguments.end()) {
            try {
                maxNoOfCOP = std::stoul(*(++itr));
            } catch (std::exception& e) {
                throw std::runtime_error("Invalid max number of COP events");
            }
        }

        itr = std::find(arguments.begin(), arguments.end(), "-r");
        if (itr != arguments.end() && itr + 1 != arguments.end()) {
            try {
                maxNoOfRace = std::stoul(*(++itr));
            } catch (std::exception& e) {
                throw std::runtime_error("Invalid max number of Races");
            }
        }

        logWitness = std::find(arguments.begin(), arguments.end(),
                               "--log-witness") != arguments.end();

        logBinaryWitness = std::find(arguments.begin(), arguments.end(),
                                     "--log-binary-witness") != arguments.end();

        binaryFormat = std::find(arguments.begin(), arguments.end(),
                                 "--human") == arguments.end();

        return {executionTrace, witnessDir, logWitness, logBinaryWitness, binaryFormat, maxNoOfCOP, maxNoOfRace};
    }
};