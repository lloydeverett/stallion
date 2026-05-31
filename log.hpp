#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class TinyLogger {
public:
    TinyLogger(LogLevel level, const char* file, int line) : msgLevel(level) {
        // Automatically prepend metadata to the buffer
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        os << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "[" << levelToString(level) << "] "
           << "[" << file << ":" << line << "] ";
    }

    // Deconstruct and flush out everything atomically to prevent interweaving threads
    ~TinyLogger() {
        static std::mutex logMutex;
        static std::ofstream logFile("stallion.log", std::ios::app);

        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) {
            logFile << os.str() << std::endl;
        }
    }

    // Expose stream capability
    template <typename T>
    TinyLogger& operator<<(const T& msg) {
        os << msg;
        return *dynamic_cast<TinyLogger*>(this);
    }

private:
    std::ostringstream os;
    LogLevel msgLevel;

    const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#ifndef NDEBUG
#define LOG_DEBUG() TinyLogger(LogLevel::DEBUG, __FILE__, __LINE__)
#define LOG_INFO()  TinyLogger(LogLevel::INFO,  __FILE__, __LINE__)
#define LOG_WARN()  TinyLogger(LogLevel::WARN,  __FILE__, __LINE__)
#define LOG_ERROR() TinyLogger(LogLevel::ERROR, __FILE__, __LINE__)
#else
#define LOG_DEBUG() while(false) std::ostream(nullptr)
#define LOG_INFO()  while(false) std::ostream(nullptr)
#define LOG_WARN()  while(false) std::ostream(nullptr)
#define LOG_ERROR() while(false) std::ostream(nullptr)
#endif

