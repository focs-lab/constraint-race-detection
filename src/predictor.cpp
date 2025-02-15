#include <mach/mach.h>
#include <sys/resource.h>

#include <iostream>

#include "casual_model.hpp"
#include "cmd_argument_parser.cpp"
#include "logger.hpp"
#include "model_logger.hpp"
#include "trace.hpp"

int main(int argc, char* argv[]) {
    try {
        auto start = std::chrono::high_resolution_clock::now();

        Arguments args = Arguments::fromArgs(argc, argv);

        std::filesystem::path inputTracePath(args.executionTrace);
        std::string witnessPath =
            args.witnessDir + "/" + inputTracePath.stem().string();

        Trace trace = args.binaryFormat
                          ? Trace::fromBinaryFile(args.executionTrace)
                          : Trace::fromTextFile(args.executionTrace);
        ModelLogger logger(trace, witnessPath, args.logWitness, args.logBinaryWitness);

        VecTransitiveClosure vc_hb(trace);

        CasualModel model(trace, vc_hb, logger,
                          args.logWitness || args.logBinaryWitness);

        uint32_t race_count = model.solve(args.maxNoOfCOP, args.maxNoOfRace);

        auto end = std::chrono::high_resolution_clock::now();

        LOG("Number of races predicted: ", race_count);
        LOG("Time taken: ",
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count());

        return 0;
    } catch (std::exception& e) {
        return 1;
    }
}