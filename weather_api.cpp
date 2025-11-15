// weather_api.cpp
#include "weather_api.h"
#include "constants.h"
#include "config.h"
#include "logger.h"
#include <iostream>
#include <ctime>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono_literals;

// Constructor: Initialize dataInitiallyFetched along with others
WeatherAPI::WeatherAPI()
    : running(false), lastUpdate(0), dataInitiallyFetched(false) { // <<< Initialize here
}

WeatherAPI::~WeatherAPI() {
    stop();
}

void WeatherAPI::start() {
    if (!running.exchange(true)) {
        updateThread = std::thread(&WeatherAPI::updateLoop, this);
    }
}

void WeatherAPI::stop() {
    if (running.exchange(false)) {
        updateCV.notify_one();
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
}

WeatherData WeatherAPI::getWeather() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return currentWeatherData;
}

// Implementation of the new method
bool WeatherAPI::isDataValid() const {
    // Read the atomic flag. No lock needed for this specific read.
    return dataInitiallyFetched.load();
}

// Note: shouldUpdate() is removed as its logic is now in updateLoop

WeatherData WeatherAPI::fetchWeatherFromAPI() {
    WeatherData result;
    
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    httplib::SSLClient cli(WEATHER_API_URL_HOST);
#else
    httplib::Client cli(WEATHER_API_URL_HOST);
#endif
    cli.set_connection_timeout(5);
    
    auto res = cli.Get(WEATHER_API_URL_PATH);
    if (!res) {
        LOG_ERROR("HTTP connection failed");
        return result;
    }
    
    if (res->status == 200) {
        try {
            json data = json::parse(res->body);
            auto current = data["current_weather"];
            result.temperature = current["temperature"].get<double>();
            result.weathercode = current["weathercode"].get<int>();
            result.windspeed = current["windspeed"].get<double>();
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing weather data: %s", e.what());
        }
    } else {
        LOG_ERROR("HTTP weather request failed with status: %d", res->status);
    }
    
    return result;
}

void WeatherAPI::updateLoop() {
    constexpr int MAX_RETRY_INTERVAL = 300;  // 5 minutes
    int retryInterval = 1;  // Start with 1 second

    while (running) {
        // Check if we should update based on time interval OR if data was never fetched
        bool needsUpdate = false;
        {
            std::lock_guard<std::mutex> lock(dataMutex); // Lock to read lastUpdate
            time_t currentTime;
            time(&currentTime);
            // Update if data hasn't been fetched yet (lastUpdate == 0) OR if interval passed
            needsUpdate = (lastUpdate == 0 || difftime(currentTime, lastUpdate) > UPDATE_INTERVAL);
        } // Lock released


        if (needsUpdate) {
            WeatherData newData = fetchWeatherFromAPI();

            if (newData.weathercode != -1) {
                // Successful update
                { // Scope for lock guard
                    std::lock_guard<std::mutex> lock(dataMutex);
                    currentWeatherData = newData;
                    time(&lastUpdate);
                    // Set the flag only after the first successful fetch
                    if (!dataInitiallyFetched.load(std::memory_order_relaxed)) { // Relaxed is fine for a flag
                         dataInitiallyFetched.store(true, std::memory_order_release); // Ensure writes are visible
                    }
                } // Lock released
                retryInterval = 1;  // Reset retry interval on success
                // updateCV.notify_one(); // Notify potentially waiting threads (optional, none currently wait on this)
            } else {
                // Failed update
                retryInterval = std::min(retryInterval * 2, MAX_RETRY_INTERVAL);
                LOG_WARNING("Weather update failed, retrying in %d seconds", retryInterval);

                // Wait for retry interval or until stopped
                std::unique_lock<std::mutex> lock(dataMutex); // Use unique_lock for condition variable
                updateCV.wait_for(lock, std::chrono::seconds(retryInterval),
                    [this]() { return !running; }); // Wait or until stop() is called
                continue; // Skip the main wait below, go straight to next loop iteration
            }
        }

        // Wait until the next update is needed OR until stop() is called
        std::unique_lock<std::mutex> lock(dataMutex); // Use unique_lock for condition variable
        // Calculate time until next update is needed based on last successful update
        time_t currentTime;
        time(&currentTime);
        double secondsToWait = UPDATE_INTERVAL; // Default wait if no data yet
        if (lastUpdate != 0) { // Avoid negative wait time if lastUpdate is 0
             // Calculate remaining time, ensure it's not negative
             secondsToWait = std::max(0.0, UPDATE_INTERVAL - difftime(currentTime, lastUpdate));
        }

        // Wait for the calculated duration OR until stop() is called OR until an update is needed (e.g., if interval passed while waiting)
        updateCV.wait_for(lock, std::chrono::seconds(static_cast<long long>(secondsToWait) + 1), // Add 1s buffer for safety
            [this]() {
                // Wake up if stop() was called
                if (!running) return true;
                // Wake up if it's time for the next update (check again under lock)
                time_t now;
                time(&now);
                return lastUpdate == 0 || difftime(now, lastUpdate) > UPDATE_INTERVAL;
             });
    }
}
