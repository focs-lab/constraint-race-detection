#include <iostream>

#include "BSlogger.hpp"
#include "casual_model.h"
#include "cmd_argument_parser.cpp"
#include "trace.h"

int main(int argc, char* argv[]) {
    try {
        LOG_INIT_COUT();

        auto start = std::chrono::high_resolution_clock::now();

        Arguments args = Arguments::fromArgs(argc, argv);

        Trace trace = args.binaryFormat
                          ? Trace::fromBinaryFile(args.executionTrace)
                          : Trace::fromTextFile(args.executionTrace);

        CasualModel model(trace);

        uint32_t race_count = model.solve();

        auto end = std::chrono::high_resolution_clock::now();

        log(LOG_INFO) << "No of races predicted: " << race_count << "\n";
        log(LOG_INFO) << "Time taken: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             end - start)
                             .count()
                      << "ms" << "\n";

        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}