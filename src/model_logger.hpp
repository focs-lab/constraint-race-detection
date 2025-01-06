#pragma once
#include <z3++.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "trace.hpp"
#include "BSlogger.hpp"

class ModelLogger {
   private:
    Trace& trace_;
    std::ofstream log_file_;
    std::ofstream binary_log_file_;

   public:
    ModelLogger(Trace& trace, const std::string& log_file_path)
        : trace_(trace) {
        std::filesystem::path log_path(log_file_path);
        std::filesystem::path dir = log_path.parent_path();

        if (!dir.empty() && !std::filesystem::exists(dir)) {
            if(!std::filesystem::create_directories(dir)) {
                throw std::runtime_error("Failed to create directory for log file");
            }
        }

        log_file_.open(log_file_path + ".txt");
        if (!log_file_.is_open()) {
            throw std::runtime_error("Failed to open log file");
        }

        binary_log_file_.open(log_file_path, std::ios::binary);
        if (!binary_log_file_.is_open()) {
            throw std::runtime_error("Failed to open binary log file");
        }
    }

    ~ModelLogger() {
        if (log_file_.is_open()) log_file_.close();
    }

    void logWitnessPrefix(const z3::model& m, const Event& e1, const Event& e2);
    void logBinaryWitnessPrefix(const z3::model& m, const Event& e1, const Event& e2);

    static std::vector<std::vector<uint32_t>> readBinaryWitness(const std::string& file_path);
};