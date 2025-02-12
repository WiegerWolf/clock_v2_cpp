// weather_api.cpp
#include "weather_api.h"
#include "constants.h"
#include "config.h"
#include <iostream>
#include <ctime>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono_literals;

WeatherAPI::WeatherAPI() 
    : running(false), lastUpdate(0) {
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

bool WeatherAPI::shouldUpdate() const {
    time_t currentTime;
    time(&currentTime);
    return difftime(currentTime, lastUpdate) > UPDATE_INTERVAL;
}

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
        std::cerr << "HTTP connection failed" << std::endl;
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
            std::cerr << "Error processing weather data: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "HTTP weather request failed with status: " << res->status << std::endl;
    }
    
    return result;
}

void WeatherAPI::updateLoop() {
    constexpr int MAX_RETRY_INTERVAL = 300;  // 5 minutes
    int retryInterval = 1;  // Start with 1 second

    while (running) {
        if (shouldUpdate()) {
            WeatherData newData = fetchWeatherFromAPI();
            
            if (newData.weathercode != -1) {
                // Successful update
                std::lock_guard<std::mutex> lock(dataMutex);
                currentWeatherData = newData;
                time(&lastUpdate);
                retryInterval = 1;  // Reset retry interval on success
            } else {
                // Failed update
                retryInterval = std::min(retryInterval * 2, MAX_RETRY_INTERVAL);
                std::cerr << "Weather update failed, retrying in " << retryInterval << " seconds" << std::endl;
                
                // Wait for retry interval or until stopped
                std::unique_lock<std::mutex> lock(dataMutex);
                updateCV.wait_for(lock, std::chrono::seconds(retryInterval), 
                    [this]() { return !running; });
                continue;
            }
        }
        
        // Wait for next update interval or until stopped
        std::unique_lock<std::mutex> lock(dataMutex);
        updateCV.wait_for(lock, std::chrono::seconds(UPDATE_INTERVAL), 
            [this]() { return !running || shouldUpdate(); });
    }
}