#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

// Thread-safe logging
inline std::mutex log_mutex;

inline std::string current_time() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S")  // Format: HH:MM:SS
        << "." << std::setfill('0') << std::setw(3) << now_ms.count(); // Milliseconds

    return oss.str();
}

// Helper function to concatenate variadic arguments into a string
template<typename... Args>
inline std::string format_log_message(Args&&... args) {
    std::ostringstream oss;
    (oss << ... << args); // Fold expression for streaming multiple arguments
    return oss.str();
}

// Logging macro with formatting support
#define LOG(...) \
    do { \
        std::lock_guard<std::mutex> lock(log_mutex); \
        std::cout << "[" << current_time() << "] " << format_log_message(__VA_ARGS__) << std::endl; \
    } while (0)

#endif // LOG_HPP
