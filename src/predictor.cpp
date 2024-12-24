#include <iostream>

#include "casual_model.h"
#include "cmd_argument_parser.cpp"
#include "trace.h"

int main(int argc, char* argv[]) {
    try {
        auto start = std::chrono::high_resolution_clock::now();

        Arguments args = Arguments::fromArgs(argc, argv);

        Trace trace = Trace::fromFile(args.executionTrace);

        CasualModel model(trace);

        uint32_t race_count = model.solve();

        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "No of races predicted: " << race_count << std::endl;
        std::cout << "Time taken: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         end - start)
                         .count()
                  << "ms" << std::endl;

        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}