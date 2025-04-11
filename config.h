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
const SDL_Color SHADOW_COLOR = {0, 0, 0, 150}; // Semi-transparent black for shadow

// Text Shadow configuration
const int SHADOW_OFFSET_X = 2;
const int SHADOW_OFFSET_Y = 2;

// Snow configuration
const int NUM_SNOWFLAKES = 1000;  // Increased number of snowflakes

// Russian language configurations
inline const std::map<int, std::string> MONTHS_RU = {
    {1, "января"}, {2, "февраля"}, {3, "марта"},
    {4, "апреля"}, {5, "мая"}, {6, "июня"},
    {7, "июля"}, {8, "августа"}, {9, "сентября"},
    {10, "октября"}, {11, "ноября"}, {12, "декабря"}
};

inline const std::map<int, std::string> WEEKDAYS_RU = {
    {0, "воскресенье"}, {1, "понедельник"}, {2, "вторник"},
    {3, "среда"}, {4, "четверг"}, {5, "пятница"},
    {6, "суббота"}
};

#endif // CONFIG_H
