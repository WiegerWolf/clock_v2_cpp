// main.cpp
#include "clock.h"
#include "logger.h"
#include <unistd.h>

int main() {
    LOG_INFO("Application starting...");
    LOG_INFO("Process ID: %d", getpid());
    Logger::instance().logMemoryUsage();
    
    try {
        Clock clock;
        LOG_INFO("Clock initialized successfully");
        clock.run();
        LOG_INFO("Clock run() completed normally");
    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal exception in main: %s", e.what());
        Logger::instance().flush();
        return 1;
    } catch (...) {
        LOG_CRITICAL("Unknown fatal exception in main");
        Logger::instance().flush();
        return 1;
    }
    
    LOG_INFO("Application shutting down normally");
    Logger::instance().logMemoryUsage();
    Logger::instance().flush();
    
    return 0;
}