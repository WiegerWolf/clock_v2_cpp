#include "logger.h"
#include <iostream>
#include <ctime>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <fstream>

Logger::Logger() : logPath("/tmp/clock_debug.log") {
    // Open log file in append mode
    logFile.open(logPath, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logPath << std::endl;
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(Level level, const char* file, int line, const char* format, ...) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Check if rotation is needed before writing
    rotateIfNeeded();
    
    if (!logFile.is_open()) {
        return; // Silently fail to avoid crashing the app
    }
    
    // Format the message using variadic arguments
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Build log entry
    std::ostringstream logEntry;
    logEntry << "[" << getCurrentTimestamp() << "] "
             << "[" << getLevelString(level) << "] "
             << "[Thread:" << getThreadId() << "] "
             << "[" << file << ":" << line << "] "
             << buffer << std::endl;
    
    // Write to file
    logFile << logEntry.str();
    
    // Flush immediately for ERROR and CRITICAL levels
    if (level >= ERROR) {
        logFile.flush();
    }
}

void Logger::logMemoryUsage() {
    std::lock_guard<std::mutex> lock(logMutex);
    
    size_t memoryKB = getMemoryUsageKB();
    
    if (!logFile.is_open()) {
        return;
    }
    
    std::ostringstream logEntry;
    logEntry << "[" << getCurrentTimestamp() << "] "
             << "[INFO] "
             << "[Thread:" << getThreadId() << "] "
             << "[memory] "
             << "RSS: " << memoryKB << " KB ("
             << (memoryKB / 1024.0) << " MB)" << std::endl;
    
    logFile << logEntry.str();
    logFile.flush();
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.flush();
    }
}

void Logger::rotateIfNeeded() {
    // Mutex should already be locked by caller
    
    size_t currentSize = getFileSize();
    if (currentSize < MAX_LOG_SIZE) {
        return; // No rotation needed
    }
    
    // Close current log file
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
    
    // Rotate existing backup files
    for (int i = MAX_ROTATED_LOGS; i > 0; i--) {
        std::string oldName = logPath + "." + std::to_string(i);
        std::string newName = logPath + "." + std::to_string(i + 1);
        
        if (i == MAX_ROTATED_LOGS) {
            // Remove oldest log
            unlink(oldName.c_str());
        } else {
            // Rename to next number
            rename(oldName.c_str(), newName.c_str());
        }
    }
    
    // Move current log to .1
    std::string backupName = logPath + ".1";
    rename(logPath.c_str(), backupName.c_str());
    
    // Open new log file
    logFile.open(logPath, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open rotated log file: " << logPath << std::endl;
    }
}

size_t Logger::getFileSize() const {
    struct stat st;
    if (stat(logPath.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

size_t Logger::getMemoryUsageKB() {
    std::ifstream statm("/proc/self/statm");
    if (!statm.is_open()) {
        return 0;
    }
    
    // Format: size resident shared text lib data dt
    // We want resident (RSS) which is the second field
    size_t size, resident;
    statm >> size >> resident;
    statm.close();
    
    // Convert pages to KB (page size is typically 4KB)
    long pageSize = sysconf(_SC_PAGESIZE);
    return (resident * pageSize) / 1024;
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::getLevelString(Level level) {
    switch (level) {
        case DEBUG:    return "DEBUG";
        case INFO:     return "INFO";
        case WARNING:  return "WARNING";
        case ERROR:    return "ERROR";
        case CRITICAL: return "CRITICAL";
        default:       return "UNKNOWN";
    }
}

unsigned long Logger::getThreadId() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return std::stoull(oss.str());
}