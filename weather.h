// weather.h
#ifndef WEATHER_H
#define WEATHER_H

#include <string>

std::string getWeatherDescription(double temperature, int weathercode, double windspeed, bool showWindspeed = true);
std::string getWindspeedType(double windspeed);

#endif // WEATHER_H