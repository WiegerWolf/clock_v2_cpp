// weather_api.h
#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

struct WeatherData {
    double temperature;
    int weathercode;
    double windspeed;

    WeatherData() : temperature(0.0), weathercode(-1), windspeed(0.0) {}
};

class WeatherAPI {
public:
    WeatherAPI();
    ~WeatherAPI();

    // Non-blocking weather data access
    WeatherData getWeather() const;

    // Check if data has been successfully fetched at least once
    bool isDataValid() const; // <<< Add this declaration

    // Control methods
    void start();  // Start background updates
    void stop();   // Stop background updates

private:
    static constexpr int UPDATE_INTERVAL = 300;  // 5 minutes in seconds

    // Thread control
    std::atomic<bool> running;
    std::thread updateThread;
    mutable std::mutex dataMutex; // Mutex protects currentWeatherData and lastUpdate
    std::condition_variable updateCV;

    // Weather data
    WeatherData currentWeatherData;
    time_t lastUpdate;
    std::atomic<bool> dataInitiallyFetched{false}; // <<< Add this flag, initialize to false

    // Internal methods
    void updateLoop();
    WeatherData fetchWeatherFromAPI();
    // bool shouldUpdate() const; // <<< This method will be removed/integrated into updateLoop

    // Prevent copying
    WeatherAPI(const WeatherAPI&) = delete;
    WeatherAPI& operator=(const WeatherAPI&) = delete;
};

#endif // WEATHER_API_H
