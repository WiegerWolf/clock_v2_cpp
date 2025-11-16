#ifndef CONSTANTS_H
#define CONSTANTS_H

// LLM Configuration (Cerebras)
extern const char* CEREBRAS_API_KEY;
extern const char* CEREBRAS_API_HOST;
extern const int CEREBRAS_API_PORT;
extern const char* CEREBRAS_API_PATH;
extern const char* CEREBRAS_MODEL;

extern const char* CLOTHING_ADVICE_LANGUAGE;

// Background Image Configuration
extern const char* BACKGROUND_API_URL_HOST;
extern const int BACKGROUND_API_URL_PORT;
extern const char* BACKGROUND_API_URL_PATH;
extern const int BACKGROUND_UPDATE_INTERVAL;
extern const double BACKGROUND_DARKNESS;

// Fallback background color (used when no image is available)
extern const int FALLBACK_BG_RED;
extern const int FALLBACK_BG_GREEN;
extern const int FALLBACK_BG_BLUE;

extern const char* WEATHER_API_URL_HOST;
extern const int WEATHER_API_URL_PORT;
extern const char* WEATHER_API_URL_PATH;
extern const char* FONT_PATH;

#endif // CONSTANTS_H
