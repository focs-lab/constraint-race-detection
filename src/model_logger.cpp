#pragma once
#include <z3++.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "event.cpp"
#include "trace.cpp"

class ModelLogger {
   private:
    Trace& trace;
    std::ofstream outputFile;

   public:
    ModelLogger(Trace& trace_, const std::string& dir,
                const std::string& outputFileName)
        : trace(trace_) {
        std::filesystem::create_directories(dir);
        std::string outputFilePath = dir + "/" + outputFileName;
        outputFile.open(outputFilePath);
        if (!outputFile.is_open()) {
            throw std::runtime_error("Could not open output file");
        }
    }

    ~ModelLogger() {
        if (outputFile.is_open()) {
            outputFile.close();
        }
    }

    void logWitness(const z3::model& m, const Event& e1, const Event& e2) {
        std::vector<Event> events = trace.getAllEvents();
        std::vector<std::pair<std::string, z3::expr>> modelValues;
        for (unsigned i = 0; i < m.size(); i++) {
            z3::func_decl v = m[i];
            modelValues.push_back({v.name().str(), m.get_const_interp(v)});
        }

        std::sort(modelValues.begin(), modelValues.end(),
                  [](const auto& a, const auto& b) {
                      return a.second.get_numeral_int() <
                             b.second.get_numeral_int();
                  });

        outputFile << "witness for: e" << e1.getEventId() << " - e"
                   << e2.getEventId() << std::endl;
        for (const auto& [name, value] : modelValues) {
            int i = std::stoi(name) - 1;
            outputFile << value << ": " << name << " - "
                       << events[i].prettyString() << std::endl;
            DEBUG_PRINT(value << ": " << name << " - "
                              << events[i].prettyString());
        }
        outputFile << "------------------------------------------------------"
                   << std::endl;
    }

    void logWitnessPrefix(const z3::model& m, const Event& e1,
                          const Event& e2) {
        std::vector<Event> events = trace.getAllEvents();
        std::vector<std::pair<std::string, int>> modelValues;
        int e1Index, e2Index;
        for (unsigned i = 0; i < m.size(); i++) {
            z3::func_decl v = m[i];
            if (v.name().str() == std::to_string(e1.getEventId())) {
                e1Index = m.get_const_interp(v).get_numeral_int();
            } else if (v.name().str() == std::to_string(e2.getEventId())) {
                e2Index = m.get_const_interp(v).get_numeral_int();
            }
            modelValues.push_back(
                {v.name().str(), m.get_const_interp(v).get_numeral_int()});
        }

        std::sort(
            modelValues.begin(), modelValues.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        outputFile << "witness for: e" << e1.getEventId() << " - e"
                   << e2.getEventId() << std::endl;

        int j = 1;
        for (const auto& [name, value] : modelValues) {
            int i = std::stoi(name) - 1;
            if (value > e1Index || value > e2Index) break;
            if (i + 1 == e1.getEventId() || i + 1 == e2.getEventId()) continue;

            outputFile << j++ << ": e" << name << " - "
                       << events[i].prettyString() << std::endl;
        }
        if (e1Index < e2Index) {
            outputFile << j++ << ": e" << e1.getEventId() << " - "
                       << e1.prettyString() << std::endl;
            outputFile << j++ << ": e" << e2.getEventId() << " - "
                       << e2.prettyString() << std::endl;
        } else {
            outputFile << j++ << ": e" << e2.getEventId() << " - "
                       << e2.prettyString() << std::endl;
            outputFile << j++ << ": e" << e1.getEventId() << " - "
                       << e1.prettyString() << std::endl;
        }
        outputFile << "------------------------------------------------------"
                   << std::endl;
    }
};