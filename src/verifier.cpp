#include <filesystem>
#include <iostream>

#include "cmd_argument_parser.cpp"
#include "model_logger.hpp"
#include "trace.hpp"

int main(int argc, char* argv[]) {
    try {
        Arguments args = Arguments::fromArgs(argc, argv);
        std::filesystem::path inputTracePath(args.executionTrace);
        std::string witnessPath =
            args.witnessDir + "/" + inputTracePath.stem().string();

        Trace trace = args.binaryFormat
                          ? Trace::fromBinaryFile(args.executionTrace)
                          : Trace::fromTextFile(args.executionTrace);

        std::vector<std::vector<uint32_t>> binaryWitness =
            ModelLogger::readBinaryWitness(witnessPath);

        for (auto witness : binaryWitness) {
            for (auto event : witness) {
                std::cout << event << "\n";
            }
            std::cout << "------------------------------------\n";
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}