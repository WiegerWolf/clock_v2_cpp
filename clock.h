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
    double currentWind;  // Add this line to track wind

    void handleEvents();
    bool shouldUpdateAdvice();
    void update();
    void draw();
    void cleanup();  // Add cleanup method
};

#endif // CLOCK_H