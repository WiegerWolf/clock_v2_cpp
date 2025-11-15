#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
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

    // Flush log buffer to disk
    void flush();

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    // Thread-safe file operations
    std::mutex logMutex;
    std::ofstream logFile;
    std::string logPath;
    static constexpr size_t MAX_LOG_SIZE = 10 * 1024 * 1024; // 10MB
    static constexpr int MAX_ROTATED_LOGS = 2;

    // Internal methods
    void rotateIfNeeded();
    size_t getFileSize() const;
    size_t getMemoryUsageKB();
    std::string getCurrentTimestamp();
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