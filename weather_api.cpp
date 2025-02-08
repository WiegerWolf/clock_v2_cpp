// weather_api.cpp
#include "weather_api.h"
#include "constants.h"
#include "config.h"
#include <iostream>
#include <ctime>
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WeatherAPI::WeatherAPI() : lastUpdate(0), updateInterval(15 * 60) {}

bool WeatherAPI::shouldUpdate() {
    time_t currentTime;
    time(&currentTime);
    return difftime(currentTime, lastUpdate) > updateInterval;
}

WeatherData WeatherAPI::fetchWeather() {
    if (!shouldUpdate() && currentWeatherData.weathercode != -1) {
        return currentWeatherData;
    }

    httplib::Client cli(WEATHER_API_URL_HOST, WEATHER_API_URL_PORT);
    cli.set_connection_timeout(5); // 5 seconds timeout
    
    auto res = cli.Get(WEATHER_API_URL_PATH);
    if (!res) {
        std::cerr << "HTTP connection failed" << std::endl;
        return currentWeatherData;
    }
    
    if (res->status == 200) {
        try {
            json data = json::parse(res->body);
            currentWeatherData.temperature = data["current_weather"]["temperature"].get<double>();
            currentWeatherData.weathercode = data["current_weather"]["weathercode"].get<int>();
            currentWeatherData.windspeed = data["current_weather"]["windspeed"].get<double>();
            time(&lastUpdate);
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error processing weather JSON: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "HTTP weather request failed with status: " << res->status << std::endl;
    }
    return currentWeatherData;
}