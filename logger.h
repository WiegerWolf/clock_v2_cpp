#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <cstdarg>

class Logger {
public:
    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    // Singleton access
    static Logger& instance();

    // Main logging function
    void log(Level level, const char* file, int line, const char* format, ...);

    // Log memory usage from /proc/self/statm
    void logMemoryUsage();

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger() = default;

    // Thread-safe console operations
    std::mutex logMutex;

    // Internal methods
    size_t getMemoryUsageKB();
    std::string getLevelString(Level level);
    unsigned long getThreadId();
};

// Convenience macros for easy logging
#define LOG_DEBUG(...) Logger::instance().log(Logger::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Logger::instance().log(Logger::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) Logger::instance().log(Logger::WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().log(Logger::ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) Logger::instance().log(Logger::CRITICAL, __FILE__, __LINE__, __VA_ARGS__)

#endif // LOGGER_H