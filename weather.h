// weather.h
#ifndef WEATHER_H
#define WEATHER_H

#include <string>
#include <string_view>
#include <unordered_map>

// Cache key for weather descriptions
struct WeatherKey {
    int temperature;      // Rounded temperature
    int weathercode;
    int windspeed;       // Rounded windspeed
    bool showWindspeed;

    bool operator==(const WeatherKey& other) const {
        return temperature == other.temperature &&
               weathercode == other.weathercode &&
               windspeed == other.windspeed &&
               showWindspeed == other.showWindspeed;
    }
};

// Hash function for WeatherKey
struct WeatherKeyHash {
    std::size_t operator()(const WeatherKey& k) const {
        return std::hash<int>{}(k.temperature) ^
               (std::hash<int>{}(k.weathercode) << 1) ^
               (std::hash<int>{}(k.windspeed) << 2) ^
               (std::hash<bool>{}(k.showWindspeed) << 3);
    }
};

// Get weather description with caching
std::string getWeatherDescription(double temperature, int weathercode, double windspeed, bool showWindspeed = true);

// Get windspeed type description (now returns string_view for efficiency)
std::string_view getWindspeedType(double windspeed);

#endif // WEATHER_H