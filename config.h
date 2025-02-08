// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <SDL_pixels.h>
#include <string>
#include <map>
#include <vector>

// Screen configuration
const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 600;

// Colors
const SDL_Color WHITE_COLOR = {255, 255, 255, 255};
const SDL_Color BLACK_COLOR = {0, 0, 0, 255};

// Snow configuration
const int NUM_SNOWFLAKES = 100;

// Weather API configuration
const char* WEATHER_API_URL_HOST = "api.open-meteo.com";
const int WEATHER_API_URL_PORT = 443;
const char* WEATHER_API_URL_PATH = "/v1/forecast?latitude=52.3738&longitude=4.8910&hourly=apparent_temperature,precipitation&current_weather=true&windspeed_unit=ms&timezone=auto";

// Clothing advice API configuration
const char* CLOTHING_ADVICE_API_KEY = "sk-ytHq6lCPc1CTX1QI0eB8Ea37622349049709404a954a7b1d"; // Set your API key here
const char* CLOTHING_ADVICE_LANGUAGE = "ru";

// Background image configuration
const char* BACKGROUND_API_URL_HOST = "peapix.com";
const int BACKGROUND_API_URL_PORT = 443;
const char* BACKGROUND_API_URL_PATH = "/bing/feed?country=us";
const int BACKGROUND_UPDATE_INTERVAL = 60 * 60; // 1 hour in seconds
const double BACKGROUND_DARKNESS = 0.5; // 0 = no darkening, 1 = completely black

// Russian language configurations
const std::map<int, std::string> MONTHS_RU = {
    {1, "января"}, {2, "февраля"}, {3, "марта"},
    {4, "апреля"}, {5, "мая"}, {6, "июня"},
    {7, "июля"}, {8, "августа"}, {9, "сентября"},
    {10, "октября"}, {11, "ноября"}, {12, "декабря"}
};

const std::vector<std::string> WEEKDAYS_RU = {
    "понедельник", "вторник", "среда",
    "четверг", "пятница", "суббота",
    "воскресенье"
};

#endif // CONFIG_H