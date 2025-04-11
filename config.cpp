#include "config.h" // Include the header to ensure consistency
#include <map>
#include <string>

// Define the Russian language configurations declared in config.h
const std::map<int, std::string> MONTHS_RU = {
    {1, "января"}, {2, "февраля"}, {3, "марта"},
    {4, "апреля"}, {5, "мая"}, {6, "июня"},
    {7, "июля"}, {8, "августа"}, {9, "сентября"},
    {10, "октября"}, {11, "ноября"}, {12, "декабря"}
};

const std::map<int, std::string> WEEKDAYS_RU = {
    {0, "воскресенье"}, {1, "понедельник"}, {2, "вторник"},
    {3, "среда"}, {4, "четверг"}, {5, "пятница"},
    {6, "суббота"}
};
