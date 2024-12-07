#include <z3++.h>

#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

#include "cmd_argument_parser.cpp"
#include "constraint_model.cpp"
#include "model_logger.cpp"
#include "trace.cpp"

std::string witness_trace_dir = "output/witness_traces";

int main(int argc, char* argv[]) {
    Parser parser(argc, argv);

    std::string filename;
    uint32_t maxNoOfCOP = 0;
    bool logWitness = parser.hasArgument("--log_witness");

    if (parser.hasArgument("-f")) {
        filename = parser.getArgument("-f");
    } else {
        std::cerr << "Please provide an input file\n";
        return 0;
    }

    if (parser.hasArgument("-c")) {
        try {
            maxNoOfCOP = std::stoul(parser.getArgument("-c"));
        } catch (std::exception& e) {
            std::cerr << "Invalid max number of COP events\n";
            return 1;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Z3_enable_trace("setup");
    // Z3_enable_trace("theory_dl");
    // Z3_enable_trace("dl_bug");
    // Z3_enable_trace("diff_logic");
    // Z3_enable_trace("ddl");
    // Z3_enable_trace("rama");
    // Z3_enable_trace("arith");

    Trace* trace = Trace::fromLog(filename);

    std::filesystem::path fs_path(filename);
    auto* logger =
        new ModelLogger(*trace, witness_trace_dir, fs_path.stem().string());

    uint32_t race_count;
    auto* m = new ConstraintModel(*trace, *logger, logWitness);

    if (maxNoOfCOP) {
        race_count = m->solveForRace(maxNoOfCOP);
    } else {
        race_count = m->solveForRace();
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Number of races detected: " << race_count << std::endl;
    std::cout << "Total time taken: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       start)
                     .count()
              << "ms" << std::endl;

    return 0;
}