#include <z3++.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include "cmd_argument_parser.cpp"
#include "custom_maximal_casual_model.cpp"
#include "event.cpp"
#include "model_logger.cpp"
#include "partial_maximal_casual_model.cpp"
#include "trace.cpp"
#include "z3_maximal_casual_model.cpp"

std::string witness_trace_dir = "output/witness_traces";

std::string custom_repr = "custom";
std::string partial_z3_expr = "partial_z3";
std::string full_z3_expr = "full_z3";

int main(int argc, char* argv[]) {
    Parser parser(argc, argv);

    std::string filename;
    std::string repr_type;
    uint32_t maxNoOfCOP;
    bool logWitness = parser.hasArgument("--log_witness");
    bool getStatistics = parser.hasArgument("--statistics");

    if (parser.hasArgument("-f")) {
        filename = parser.getArgument("-f");
    } else {
        std::cerr << "Please provide an input file\n";
        return 0;
    }

    if (parser.hasArgument("-r")) {
        repr_type = parser.getArgument("-r");

        if (repr_type != custom_repr && repr_type != partial_z3_expr &&
            repr_type != full_z3_expr) {
            std::cerr << "Invalid representation type\n";
            return 1;
        }
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

    Trace* trace = Trace::fromLog(filename);

    std::filesystem::path fs_path(filename);
    ModelLogger* logger =
        new ModelLogger(*trace, witness_trace_dir, fs_path.stem().string());
    if (trace == nullptr) {
        std::cerr << "Error parsing log file" << std::endl;
        return 1;
    }

    uint32_t race_count;
    Model* m;

    if (repr_type == custom_repr) {
        m = new CustomMaximalCasualModel(*trace);
    } else if (repr_type == partial_z3_expr) {
        m = new PartialMaximalCasualModel(*trace);
    } else {
        m = new Z3MaximalCasualModel(*trace, *logger, logWitness);
    }

    if (getStatistics) {
        m->getStatistics();
    } else if (maxNoOfCOP) {
        race_count = m->solveForRace(maxNoOfCOP);
    } else {
        race_count = m->solveForRace();
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (!getStatistics)
        std::cout << "Number of races detected: " << race_count << std::endl;
    
    std::cout << "Total time taken: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       start)
                     .count()
              << "ms" << std::endl;

    return 0;
}