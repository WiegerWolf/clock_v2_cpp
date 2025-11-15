#include "logger.h"
#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <fstream>
#include <functional>
#include <string>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(Level level, const char* file, int line, const char* format, ...) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Format the message using variadic arguments
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Build log entry without timestamp (systemd provides it)
    std::ostringstream logEntry;
    logEntry << "[" << getLevelString(level) << "] "
             << "[Thread:" << getThreadId() << "] "
             << "[" << file << ":" << line << "] "
             << buffer << std::endl;
    
    // Route ERROR and CRITICAL to stderr, others to stdout
    if (level >= ERROR) {
        std::cerr << logEntry.str() << std::flush;
    } else {
        std::cout << logEntry.str() << std::flush;
    }
}

void Logger::logMemoryUsage() {
    std::lock_guard<std::mutex> lock(logMutex);
    
    size_t memoryKB = getMemoryUsageKB();
    
    std::ostringstream logEntry;
    logEntry << "[INFO] "
             << "[Thread:" << getThreadId() << "] "
             << "[memory] "
             << "RSS: " << memoryKB << " KB ("
             << (memoryKB / 1024.0) << " MB)" << std::endl;
    
    std::cout << logEntry.str() << std::flush;
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
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}