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

Clock::Clock() : running(false), window(nullptr), renderer(nullptr), display(nullptr), snow(nullptr), weatherAPI(nullptr), backgroundManager(nullptr), lastAdviceUpdate(0), adviceUpdateInterval(15 * 60), clothingAdvice("") {}

Clock::~Clock() {
    running = false;  // Stop the main loop first
    
    // Delete snow system first since it depends on the renderer
    if (snow) {
        delete snow;
        snow = nullptr;
    }
    
    // Delete display next as it also uses renderer
    if (display) {
        delete display;
        display = nullptr;
    }
    
    // Stop and delete weather API
    if (weatherAPI) {
        weatherAPI->stop();
        delete weatherAPI;
        weatherAPI = nullptr;
    }
    
    // Delete background manager
    if (backgroundManager) {
        delete backgroundManager;
        backgroundManager = nullptr;
    }
    
    // Finally clean up SDL resources
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

bool Clock::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Hide the mouse cursor
    SDL_ShowCursor(SDL_DISABLE);

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
    display->setFpsVisible(false);  // Hide FPS counter
    
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
        // SDL_Delay(1); // Removed: VSync should handle frame pacing
    }
}

void Clock::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                SDL_ShowCursor(SDL_DISABLE);
            }
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

    snow->update(); // Update snow physics
    backgroundManager->update(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Check if weather data is valid *before* deciding to update advice
    if (weatherAPI->isDataValid()) {
        // Only update advice if data is valid AND the interval has passed
        if (shouldUpdateAdvice()) {
            WeatherData currentWeatherData = weatherAPI->getWeather();
            clothingAdvice = getClothingAdvice(
                currentWeatherData.temperature,
                currentWeatherData.weathercode,
                currentWeatherData.windspeed,
                CLOTHING_ADVICE_LANGUAGE
            );
            time(&lastAdviceUpdate); // Update timestamp only after successful advice fetch
        }
    } else if (clothingAdvice.empty()) {
        // Optional: Set a temporary message while waiting for the first fetch
        // This prevents showing nothing while waiting for the initial data.
        clothingAdvice = "Получение данных..."; // Or "Loading data..."
    }
}

void Clock::draw() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw background first
    backgroundManager->draw(renderer);

    // Draw snow on top of background
    snow->draw(renderer);

    // Text capture removed - Draw directly to the main target
    // Draw all text elements
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&currentTime);

    // Draw time
    std::stringstream timeStream;
    timeStream << std::setfill('0') << std::setw(2) << now_tm->tm_hour << ":" << std::setfill('0') << std::setw(2) << now_tm->tm_min;
    display->renderText(
        timeStream.str(),
        display->fontLarge,
        WHITE_COLOR,
        SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - SCREEN_HEIGHT / 10
    );

    // Draw date
    std::stringstream dateStream;
    dateStream << WEEKDAYS_RU.at(now_tm->tm_wday) << ", " << now_tm->tm_mday << " " << MONTHS_RU.at(now_tm->tm_mon + 1) << " " << (now_tm->tm_year + 1900) << " года";
    display->renderText(
        dateStream.str(),
        display->fontSmall,
        WHITE_COLOR,
        SCREEN_WIDTH / 2, SCREEN_HEIGHT * 0.075
    );

    // Draw weather
    WeatherData currentWeatherData = weatherAPI->getWeather();
    std::string weatherStr = getWeatherDescription(
        currentWeatherData.temperature,
        currentWeatherData.weathercode,
        currentWeatherData.windspeed
    );
    int weatherY = SCREEN_HEIGHT * 0.75;
    display->renderText(
        weatherStr,
        display->fontSmall,
        WHITE_COLOR,
        SCREEN_WIDTH / 2, weatherY
    );

    // Draw clothing advice
    if (!clothingAdvice.empty()) {
        int adviceY = weatherY + TTF_FontLineSkip(display->fontSmall) * 1.2;
        display->renderMultilineText(
            clothingAdvice,
            display->fontExtraSmall,
            WHITE_COLOR,
            SCREEN_WIDTH / 2,
            adviceY,
            1.2f
        );
    }

    // Text capture removed
    // No need to reset texture change flag
     
    // Update and render FPS counter (which also draws itself)
    display->update();
    
    SDL_RenderPresent(renderer);
}
