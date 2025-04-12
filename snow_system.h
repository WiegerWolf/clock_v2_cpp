// snow_system.h
#ifndef SNOW_SYSTEM_H
#define SNOW_SYSTEM_H

#include <SDL.h>
#include <vector>
// #include "display.h" // No longer needed for update

// Keep Snowflake struct only for the pre-rendering phase
struct Snowflake {
    float x, y, speed, drift;
    // float alpha;     // Opacity - Removed
    float angle;        // Current rotation angle
    float angleVel;     // Angular velocity
    int radius;
    // bool settled;    // Removed
    // int settleTime; // Removed
    float depth;        // Z-position: -1.0 (behind) to 1.0 (in front) - Used for sorting during pre-render
};


class SnowSystem {
public:
    SnowSystem(int numFlakes, int screenWidth, int screenHeight);
    ~SnowSystem();

    void initialize(SDL_Renderer* renderer);
    void update(); // Removed wind and display parameters
    void draw(SDL_Renderer* renderer);

private:
    // Keep createSnowflake only for pre-rendering phase
    Snowflake createSnowflake(int screenWidth, int screenHeight);
    // Removed vertex buffer methods

    // std::vector<Snowflake> snowflakes; // Temporary during initialization only
    int numFlakes; // Store the requested number
    int screenWidth, screenHeight;
    SDL_Renderer* renderer; // Keep renderer pointer

    // Pre-rendered frames
    std::vector<SDL_Texture*> preRenderedFrames;
    int currentFrameIndex;
    int totalFrames;

    // Constants for pre-rendering
    static const int PRE_RENDER_SECONDS = 30; // Duration of the animation loop
    static const int PRE_RENDER_FPS = 30;     // FPS for the pre-rendered animation
    static const int FADE_FRAMES = PRE_RENDER_FPS * 2; // 2 second fade duration
};

#endif // SNOW_SYSTEM_H
