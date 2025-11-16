// main.cpp
#include "clock.h"
#include "logger.h"
#include <unistd.h>
#include "version.h"

int main() {
    LOG_INFO("Application starting...");
    LOG_INFO("Version: %s (built %s)", VERSION_GIT_HASH, VERSION_BUILD_TIME);
    LOG_INFO("Process ID: %d", getpid());
    Logger::instance().logMemoryUsage();
    
    try {
        Clock clock;
        LOG_INFO("Clock initialized successfully");
        clock.run();
        LOG_INFO("Clock run() completed normally");
    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal exception in main: %s", e.what());
        return 1;
    } catch (...) {
        LOG_CRITICAL("Unknown fatal exception in main");
        return 1;
    }
    
    LOG_INFO("Application shutting down normally");
    Logger::instance().logMemoryUsage();
    
    return 0;
}