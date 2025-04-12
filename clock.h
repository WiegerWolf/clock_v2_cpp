// clock.h
#ifndef CLOCK_H
#define CLOCK_H

#include <SDL.h>
#include <string>

class Display;
class SnowSystem;
class WeatherAPI;
class BackgroundManager;

class Clock {
public:
    Clock();
    ~Clock();
    bool initialize();
    void run();

private:
    bool running;
    SDL_Window* window;
    SDL_Renderer* renderer;
    Display* display;
    SnowSystem* snow;
    WeatherAPI* weatherAPI;
    BackgroundManager* backgroundManager;
    time_t lastAdviceUpdate;
    int adviceUpdateInterval;
    std::string clothingAdvice;
    // double currentWind; // Removed - snow is pre-rendered

    void handleEvents();
    bool shouldUpdateAdvice();
    void update();
    void draw();
};

#endif // CLOCK_H
