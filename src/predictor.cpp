#include <mach/mach.h>
#include <sys/resource.h>

#include <iostream>
#include <memory>

#include "cmd_argument_parser.cpp"
#include "logger.hpp"
#include "model.hpp"
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
        ModelLogger logger(trace, witnessPath, args.logWitness,
                           args.logBinaryWitness);

        VecTransitiveClosure vc_hb(trace);

        auto model =
            Model::createModel(args.modelType, trace, vc_hb, logger,
                               args.logWitness || args.logBinaryWitness);

        uint32_t race_count = model->solve(args.maxNoOfCOP, args.maxNoOfRace);

        auto end = std::chrono::high_resolution_clock::now();

        LOG("====================RESULTS=====================");
        LOG("Model type: ", args.modelType);
        LOG("Number of races predicted: ", race_count);
        LOG("Time taken: ",
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count(),
            " ms");
        LOG("================================================");

        return 0;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}