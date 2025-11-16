#include "constants.h"

// --- LLM Configuration (Cerebras) ---
#ifndef CEREBRAS_API_KEY_DEFINE
#error "CEREBRAS_API_KEY_DEFINE is not set. Provide it via CMake -DCEREBRAS_API_KEY_DEFINE='your-key'"
#endif
const char* CEREBRAS_API_KEY = CEREBRAS_API_KEY_DEFINE;
const char* CEREBRAS_API_HOST = "api.cerebras.ai";
const int CEREBRAS_API_PORT = 443;
const char* CEREBRAS_API_PATH = "/v1/chat/completions";
const char* CEREBRAS_MODEL = "gpt-oss-120b";

const char* CLOTHING_ADVICE_LANGUAGE = "ru";

// --- Background Image Configuration ---
const char* BACKGROUND_API_URL_HOST = "peapix.com";
const int BACKGROUND_API_URL_PORT = 443;
const char* BACKGROUND_API_URL_PATH = "/bing/feed?country=us";
const int BACKGROUND_UPDATE_INTERVAL = 60 * 60; // 1 hour in seconds
const double BACKGROUND_DARKNESS = 0.3; // 0 = no darkening, 1 = completely black

// Fallback background color (dark blue - pleasant and doesn't interfere with text)
const int FALLBACK_BG_RED = 25;
const int FALLBACK_BG_GREEN = 35;
const int FALLBACK_BG_BLUE = 55;

const char* WEATHER_API_URL_HOST = "api.open-meteo.com";
const int WEATHER_API_URL_PORT = 443;
const char* WEATHER_API_URL_PATH = "/v1/forecast?latitude=52.3738&longitude=4.8910&hourly=apparent_temperature,precipitation&current_weather=true&windspeed_unit=ms&timezone=auto";

const char* FONT_PATH = "assets/fonts/BellotaText-Bold.ttf";
