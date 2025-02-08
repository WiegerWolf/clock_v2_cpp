// weather_api.h
#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <string>

struct WeatherData {
    double temperature;
    int weathercode;
    double windspeed;

    WeatherData() : temperature(0.0), weathercode(-1), windspeed(0.0) {} // Default constructor
};

class WeatherAPI {
public:
    WeatherAPI();
    WeatherData fetchWeather();

private:
    time_t lastUpdate;
    WeatherData currentWeatherData;
    int updateInterval;
    bool shouldUpdate();
};

#endif // WEATHER_API_H