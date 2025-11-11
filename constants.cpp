#include "constants.h"

// --- LLM Configuration (OpenRouter) ---
#ifndef OPENROUTER_API_KEY_DEFINE
#error "OPENROUTER_API_KEY_DEFINE is not set. Provide it via CMake -DOPENROUTER_API_KEY_DEFINE='your-key'"
#endif
const char* OPENROUTER_API_KEY = OPENROUTER_API_KEY_DEFINE;
const char* OPENROUTER_API_HOST = "openrouter.ai";
const int OPENROUTER_API_PORT = 443;
const char* OPENROUTER_API_PATH = "/api/v1/chat/completions";
const char* OPENROUTER_MODEL = "openai/gpt-oss-120b";
const char* OPENROUTER_REFERER = "https://clock.tsatsin.com";
const char* OPENROUTER_TITLE = "Digital clock";
const char* CLOTHING_ADVICE_LANGUAGE = "ru";

// --- Background Image Configuration ---
const char* BACKGROUND_API_URL_HOST = "peapix.com";
const int BACKGROUND_API_URL_PORT = 443;
const char* BACKGROUND_API_URL_PATH = "/bing/feed?country=us";
const int BACKGROUND_UPDATE_INTERVAL = 60 * 60; // 1 hour in seconds
const double BACKGROUND_DARKNESS = 0.3; // 0 = no darkening, 1 = completely black

const char* WEATHER_API_URL_HOST = "api.open-meteo.com";
const int WEATHER_API_URL_PORT = 443;
const char* WEATHER_API_URL_PATH = "/v1/forecast?latitude=52.3738&longitude=4.8910&hourly=apparent_temperature,precipitation&current_weather=true&windspeed_unit=ms&timezone=auto";

const char* FONT_PATH = "assets/fonts/BellotaText-Bold.ttf";
