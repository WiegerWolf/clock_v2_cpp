// clothing_advice.h
#ifndef CLOTHING_ADVICE_H
#define CLOTHING_ADVICE_H

#include <string>

// Updated function signature: apiKey parameter removed
std::string getClothingAdvice(double temperature, int weathercode, double windspeed, const char* language = "ru");
std::string getBasicAdvice(double temperature);

#endif // CLOTHING_ADVICE_H
