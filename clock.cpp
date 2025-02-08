// clock.cpp
#include "clock.h"
#include "display.h"
#include "snow_system.h"
#include "weather_api.h"
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
    if (display) delete display;
    if (snow) delete snow;
    if (weatherAPI) delete weatherAPI;
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
    weatherAPI = new WeatherAPI();
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
    double wind = 0.2 * sin(static_cast<double>(currentTime)); // Using ctime for timestamp

    snow->update(wind);

    WeatherData currentWeatherData = weatherAPI->fetchWeather();

    if (shouldUpdateAdvice()) {
        clothingAdvice = getClothingAdvice(
            currentWeatherData.temperature,
            currentWeatherData.weathercode,
            currentWeatherData.windspeed,
            CLOTHING_ADVICE_API_KEY,
            CLOTHING_ADVICE_LANGUAGE
        );
        time(&lastAdviceUpdate);
    }

    display->clear();

    // Draw time
    std::stringstream timeStream;
    timeStream << std::setfill('0') << std::setw(2) << now_tm->tm_hour << ":" << std::setfill('0') << std::setw(2) << now_tm->tm_min;
    display->renderText(
        timeStream.str(),
        display->fontLarge,
        WHITE_COLOR,
        SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2
    );

    // Draw date
    std::stringstream dateStream;
    dateStream << WEEKDAYS_RU.at(now_tm->tm_wday) << ", " << now_tm->tm_mday << " " << MONTHS_RU.at(now_tm->tm_mon + 1) << " " << (now_tm->tm_year + 1900) << " года";
    display->renderText(
        dateStream.str(),
        display->fontSmall,
        WHITE_COLOR,
        SCREEN_WIDTH / 2, SCREEN_HEIGHT * 0.1
    );

    // Draw weather
    std::string weatherStr = getWeatherDescription(
        currentWeatherData.temperature,
        currentWeatherData.weathercode,
        currentWeatherData.windspeed
    );
    int weatherY = SCREEN_HEIGHT * 0.8;
    display->renderText(
        weatherStr,
        display->fontSmall,
        WHITE_COLOR,
        SCREEN_WIDTH / 2, weatherY
    );

    // Draw clothing advice
    if (!clothingAdvice.empty()) {
        int adviceY = weatherY + TTF_FontLineSkip(display->fontSmall) * 1.5;
        display->renderMultilineText(
            clothingAdvice,
            display->fontSmall,
            WHITE_COLOR,
            SCREEN_WIDTH / 2,
            adviceY,
            1.2f
        );
    }

    snow->draw(renderer);
}

void Clock::draw() {
    SDL_RenderPresent(renderer);
}