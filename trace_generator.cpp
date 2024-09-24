#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <filesystem>

#include "event.cpp"

const std::string output_dir = "./formatted_logs";

std::unordered_map<std::string, Event::EventType> eventTypeMapping= {
    {"Read", Event::EventType::Read},
    {"Write", Event::EventType::Write},
    {"Acq", Event::EventType::Acquire},
    {"Rel", Event::EventType::Release},
    {"Begin", Event::EventType::Begin},
    {"End", Event::EventType::End},
    {"Fork", Event::EventType::Fork},
    {"Join", Event::EventType::Join}
};

std::unordered_map<std::string, uint32_t> varNameMapping;
uint32_t availVarId = 0;

uint64_t parseLineToRawEvent(const std::string& line) {
    std::istringstream iss(line);
    std::string eventTypeStr, varName;
    uint32_t threadId, varValue, varId;

    if(!(iss >> eventTypeStr >> threadId >> varName >> varValue)) {
        throw std::runtime_error("Error parsing line: " + line);
    }

    Event::EventType eventType = eventTypeMapping[eventTypeStr];
    
    if (varNameMapping.find(varName) != varNameMapping.end()) {
        varId = varNameMapping[varName];
    } else {
        varId = varNameMapping[varName] = availVarId++;
    }

    return Event::createRawEvent(eventType, threadId, varId, varValue);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Please provide an input file\n";
        return 0;
    }

    std::string inputFilePath = argv[1];

    std::filesystem::path inputPath(inputFilePath);
    std::string filename = inputPath.filename().string();
    std::filesystem::path outputFilePath = std::filesystem::path(output_dir) / filename;

    std::ifstream inputFile(inputFilePath);
    std::ofstream outputFile(outputFilePath);

    if (!inputFile.is_open() || !outputFile.is_open()) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        try {
            uint64_t rawEvent = parseLineToRawEvent(line);
            outputFile << rawEvent << std::endl;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    inputFile.close();
    outputFile.close();

    std::cout << "Generated formatted logs" << std::endl;

    return 0;
}