// weather.cpp
#include "weather.h"
#include "config.h"
#include <string>
#include <sstream>
#include <cmath>
#include <map>
#include <unordered_map>

// Weather code descriptions using string_view for efficiency
const std::map<int, std::string_view> WEATHER_CODE_RU = {
    {0, "Ясно"},
    {1, "Редкие облака"},
    {2, "Переменная облачность"},
    {3, "Облачно"},
    {45, "Туман"},
    {48, "Изморозь"},
    {51, "Легкая морось"},
    {53, "Моросит"},
    {55, "Плотно моросит"},
    {56, "Ледяная морось"},
    {57, "Тяжелая ледяная морось"},
    {61, "Легкий дождик"},
    {63, "Дождь"},
    {65, "Ливень"},
    {66, "Холодный дождь"},
    {67, "Ледяной ливень"},
    {71, "Снежок"},
    {73, "Снегопад"},
    {75, "Сильный снегопад"},
    {77, "Снежный град"},
    {80, "Ливневый дождик"},
    {81, "Ливни"},
    {82, "Плотные ливни"},
    {85, "Снежный дождик"},
    {86, "Снежные дожди"},
    {95, "Небольшая гроза"},
    {96, "Гроза с маленьким градом"},
    {99, "Град с грозой"}
};

// Windspeed type descriptions using string_view
std::string_view getWindspeedType(double windspeed) {
    if (windspeed < 1) {
        return "штиль";
    } else if (windspeed <= 5) {
        return "ветерок";
    } else if (windspeed <= 10) {
        return "ветер";
    } else if (windspeed <= 15) {
        return "сильный ветер";
    } else if (windspeed <= 20) {
        return "шквальный ветер";
    } else {
        return "ураган";
    }
}

// Static cache for weather descriptions
static std::unordered_map<WeatherKey, std::string, WeatherKeyHash> weatherCache;
static const size_t MAX_CACHE_SIZE = 1000;

std::string getWeatherDescription(double temperature, int weathercode, double windspeed, bool showWindspeed) {
    // Round values for cache key
    int roundedTemp = static_cast<int>(std::round(temperature));
    int roundedWind = static_cast<int>(std::round(windspeed));
    
    // Create cache key
    WeatherKey key{roundedTemp, weathercode, roundedWind, showWindspeed};
    
    // Check cache
    auto it = weatherCache.find(key);
    if (it != weatherCache.end()) {
        return it->second;
    }
    
    // Clean cache if too large
    if (weatherCache.size() >= MAX_CACHE_SIZE) {
        weatherCache.clear();
    }
    
    // Build description
    std::string description;
    description.reserve(50); // Pre-allocate space for typical description length
    
    // Add temperature
    description += std::to_string(roundedTemp);
    description += "°C";
    
    // Add weather code description
    if (auto it = WEATHER_CODE_RU.find(weathercode); it != WEATHER_CODE_RU.end()) {
        description += ", ";
        description += it->second;
    }
    
    // Add windspeed if requested
    if (showWindspeed) {
        description += ", ";
        description += getWindspeedType(windspeed);
        if (windspeed >= 1) {
            description += " ";
            description += std::to_string(roundedWind);
            description += " м/с";
        }
    }
    
    // Cache and return
    weatherCache[key] = description;
    return description;
}