#include <iostream>
#include <sys/resource.h>
#include <mach/mach.h>


#include "BSlogger.hpp"
#include "casual_model.hpp"
#include "cmd_argument_parser.cpp"
#include "trace.hpp"
#include "model_logger.hpp"

void printMemoryUsage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    // Print max stack usage
    std::cout << "Max Stack Usage: " << usage.ru_maxrss << " KB\n";

    // Get heap memory usage using macOS-specific API
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        std::cout << "Heap Usage: " << info.resident_size / (1024 * 1024) << " MB\n";
    }
}

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

        ModelLogger logger(trace, witnessPath, args.logBinaryWitness);

        CasualModel model(trace, logger, args.logWitness);

        uint32_t race_count = model.solve(args.maxNoOfCOP, args.maxNoOfRace);

        auto end = std::chrono::high_resolution_clock::now();

        log(LOG_INFO) << "No of races predicted: " << race_count << "\n";
        log(LOG_INFO) << "Time taken: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             end - start)
                             .count()
                      << "ms" << "\n";

        printMemoryUsage();
        return 0;
    } catch (std::exception& e) {
        log(LOG_ERROR) << e.what() << "\n";
        printMemoryUsage();
        return 1;
    }
}