#include <iostream>

#include "BSlogger.hpp"
#include "casual_model.hpp"
#include "cmd_argument_parser.cpp"
#include "trace.hpp"
#include "model_logger.hpp"

int main(int argc, char* argv[]) {
    LOG_INIT_COUT();
    try {
        auto start = std::chrono::high_resolution_clock::now();

        Arguments args = Arguments::fromArgs(argc, argv);

        std::filesystem::path inputTracePath(args.executionTrace);
        std::string witnessPath = args.witnessDir + "/" + inputTracePath.stem().string();

        Trace trace = args.binaryFormat
                          ? Trace::fromBinaryFile(args.executionTrace)
                          : Trace::fromTextFile(args.executionTrace);

        ModelLogger logger(trace, witnessPath);

        CasualModel model(trace, logger, args.logWitness, args.logBinaryWitness);

        uint32_t race_count = model.solve(args.maxNoOfCOP, args.maxNoOfRace);

        auto end = std::chrono::high_resolution_clock::now();

        log(LOG_INFO) << "No of races predicted: " << race_count << "\n";
        log(LOG_INFO) << "Time taken: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             end - start)
                             .count()
                      << "ms" << "\n";

        return 0;
    } catch (std::exception& e) {
        log(LOG_ERROR) << e.what() << "\n";
        return 1;
    }
}