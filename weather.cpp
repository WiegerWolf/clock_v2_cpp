// weather.cpp
#include "weather.h"
#include "config.h"
#include <string>
#include <sstream>
#include <cmath>
#include <map>

const std::map<int, std::string> WEATHER_CODE_RU = {
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

std::string getWindspeedType(double windspeed) {
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

std::string getWeatherDescription(double temperature, int weathercode, double windspeed, bool showWindspeed) {
    std::stringstream descriptionStream;
    descriptionStream << static_cast<int>(std::round(temperature)) << "°C";

    if (WEATHER_CODE_RU.count(weathercode)) {
        descriptionStream << ", " << WEATHER_CODE_RU.at(weathercode);
    }

    if (showWindspeed) {
        std::string windspeedType = getWindspeedType(windspeed);
        descriptionStream << ", " << windspeedType;
        if (windspeed >= 1) {
            descriptionStream << " " << static_cast<int>(std::round(windspeed)) << " м/с";
        }
    }

    return descriptionStream.str();
}