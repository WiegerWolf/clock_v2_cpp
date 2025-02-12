// clock.cpp
#include "clock.h"
#include "display.h"
#include "snow_system.h"
#include "weather_api.h"
#include "constants.h"
#include "background_manager.h"
#include "config.h"
#include "weather.h"
#include "clothing_advice.h"
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <iostream>
#include <cmath>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

Clock::Clock() : running(false), window(nullptr), renderer(nullptr), display(nullptr), snow(nullptr), weatherAPI(nullptr), backgroundManager(nullptr), lastAdviceUpdate(0), adviceUpdateInterval(15 * 60), clothingAdvice(""), currentWind(0.0) {}

Clock::~Clock() {
    if (weatherAPI) {
        weatherAPI->stop();  // Stop weather updates before deletion
        delete weatherAPI;
    }
    if (display) delete display;
    if (snow) delete snow;
    if (backgroundManager) delete backgroundManager;
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

bool Clock::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0) {
        std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    window = SDL_CreateWindow("Digital Clock C++", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    display = new Display(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    snow = new SnowSystem(NUM_SNOWFLAKES, SCREEN_WIDTH, SCREEN_HEIGHT);
    snow->initialize(renderer);  // Initialize snow system with renderer
    weatherAPI = new WeatherAPI();
    weatherAPI->start();  // Start background weather updates
    backgroundManager = new BackgroundManager();

    running = true;
    return true;
}

void Clock::run() {
    if (!initialize()) {
        return;
    }

    while (running) {
        handleEvents();
        update();
        draw();
        SDL_Delay(16); // ~60 FPS
    }
}

void Clock::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
    }
}

bool Clock::shouldUpdateAdvice() {
    time_t currentTime;
    time(&currentTime);
    return difftime(currentTime, lastAdviceUpdate) > adviceUpdateInterval;
}

void Clock::update() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&currentTime);
    currentWind = 0.2 * sin(static_cast<double>(currentTime));

    snow->update(currentWind, display);
    backgroundManager->update(SCREEN_WIDTH, SCREEN_HEIGHT);

    if (shouldUpdateAdvice()) {
        WeatherData currentWeatherData = weatherAPI->getWeather();
        clothingAdvice = getClothingAdvice(
            currentWeatherData.temperature,
            currentWeatherData.weathercode,
            currentWeatherData.windspeed,
            CLOTHING_ADVICE_API_KEY,
            CLOTHING_ADVICE_LANGUAGE
        );
        time(&lastAdviceUpdate);
    }
}

void Clock::draw() {
    static std::string lastTimeStr;
    static std::string lastDateStr;
    static std::string lastWeatherStr;
    static std::string lastAdviceStr;
    static int lastMinute = -1;
    static int lastDay = -1;
    static bool needsFullRedraw = true;
    
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&currentTime);

    // Check if we need a full redraw
    if (now_tm->tm_min != lastMinute || display->hasTextureChanged()) {
        needsFullRedraw = true;
        lastMinute = now_tm->tm_min;
    }

    if (needsFullRedraw) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        backgroundManager->draw(renderer);

        // Begin capturing text renders
        display->beginTextCapture();
        
        // Update and draw time if changed
        std::stringstream timeStream;
        timeStream << std::setfill('0') << std::setw(2) << now_tm->tm_hour << ":"
                  << std::setfill('0') << std::setw(2) << now_tm->tm_min;
        std::string timeStr = timeStream.str();
        if (timeStr != lastTimeStr) {
            display->renderText(
                timeStr,
                display->fontLarge,
                WHITE_COLOR,
                SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                false  // Not dynamic since we control updates
            );
            lastTimeStr = timeStr;
        }

        // Update and draw date if day changed
        if (now_tm->tm_mday != lastDay) {
            std::stringstream dateStream;
            dateStream << WEEKDAYS_RU.at(now_tm->tm_wday) << ", " << now_tm->tm_mday << " "
                      << MONTHS_RU.at(now_tm->tm_mon + 1) << " " << (now_tm->tm_year + 1900) << " года";
            std::string dateStr = dateStream.str();
            if (dateStr != lastDateStr) {
                display->renderText(
                    dateStr,
                    display->fontSmall,
                    WHITE_COLOR,
                    SCREEN_WIDTH / 2, SCREEN_HEIGHT * 0.1,
                    false
                );
                lastDateStr = dateStr;
            }
            lastDay = now_tm->tm_mday;
        }

        // Draw weather if changed
        WeatherData currentWeatherData = weatherAPI->getWeather();
        std::string weatherStr = getWeatherDescription(
            currentWeatherData.temperature,
            currentWeatherData.weathercode,
            currentWeatherData.windspeed
        );
        if (weatherStr != lastWeatherStr) {
            int weatherY = SCREEN_HEIGHT * 0.8;
            display->renderText(
                weatherStr,
                display->fontSmall,
                WHITE_COLOR,
                SCREEN_WIDTH / 2, weatherY,
                false
            );
            lastWeatherStr = weatherStr;
        }

        // Draw clothing advice if changed
        if (!clothingAdvice.empty() && clothingAdvice != lastAdviceStr) {
            int weatherY = SCREEN_HEIGHT * 0.8;
            int adviceY = weatherY + TTF_FontLineSkip(display->fontSmall) * 1.5;
            display->renderMultilineText(
                clothingAdvice,
                display->fontSmall,
                WHITE_COLOR,
                SCREEN_WIDTH / 2,
                adviceY,
                1.2f
            );
            lastAdviceStr = clothingAdvice;
        }
        
        display->endTextCapture();
        needsFullRedraw = false;
    }

    // Draw snow before presenting
    snow->draw(renderer);
    
    // Update FPS counter
    display->update();
    
    SDL_RenderPresent(renderer);
}