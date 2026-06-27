#pragma once

#include <chrono>
#include <format>
#include <fstream>
#include <mutex>
#include <string>

constexpr const char *LOG_FILENAME = "stallion.log";

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

inline void tiny_log(LogLevel level, const char *file, int line,
                     std::string msg) {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  const char *level_str;
  switch (level) {
  case LogLevel::DEBUG:
    level_str = "DEBUG";
    break;
  case LogLevel::INFO:
    level_str = "INFO";
    break;
  case LogLevel::WARN:
    level_str = "WARN";
    break;
  case LogLevel::ERROR:
    level_str = "ERROR";
    break;
  default:
    level_str = "UNKNOWN";
    break;
  }

  char time_buf[32];
  std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
                std::localtime(&time));

  auto line_str = std::format("[{}.{:03d}] [{}] [{}:{}] {}\n", time_buf,
                              ms.count(), level_str, file, line, msg);

  static std::mutex log_mutex;
  static std::ofstream log_file(LOG_FILENAME, std::ios::app);
  std::lock_guard<std::mutex> lock(log_mutex);
  if (log_file.is_open()) {
    log_file << line_str;
  }
}

template <typename... Args>
void tiny_log_fmt(LogLevel level, const char *file, int line,
                  std::format_string<Args...> fmt, Args &&...args) {
  tiny_log(level, file, line, std::format(fmt, std::forward<Args>(args)...));
}

#ifndef NDEBUG
#define LOG_DEBUG(...)                                                         \
  tiny_log_fmt(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
  } while (false)
#endif
#define LOG_INFO(...)                                                          \
  tiny_log_fmt(LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)                                                          \
  tiny_log_fmt(LogLevel::WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
  tiny_log_fmt(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
