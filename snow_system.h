// snow_system.h
#ifndef SNOW_SYSTEM_H
#define SNOW_SYSTEM_H

#include <SDL.h>
#include <vector>
#include <random> // Include for random number generation in createSnowflake

struct Snowflake {
    float x, y, speed, drift;
    float angle;        // Current rotation angle
    float angleVel;     // Angular velocity
    int radius;
    float depth;        // Z-position: -1.0 (behind) to 1.0 (in front) - Used for sorting
};

class SnowSystem {
public:
    SnowSystem(int numFlakes, int screenWidth, int screenHeight);
    ~SnowSystem();

    void initialize(SDL_Renderer* renderer);
    void update(); // No wind parameter for now
    void draw(SDL_Renderer* renderer);

private:
    Snowflake createSnowflake(int screenWidth, int screenHeight);
    SDL_Texture* createCircleTexture(SDL_Renderer* renderer, int radius, Uint8 alpha); // Helper

    std::vector<Snowflake> snowflakes;
    int numFlakes;
    int screenWidth, screenHeight;
    SDL_Renderer* renderer; // Keep renderer pointer

    // Base textures for different snowflake sizes
    SDL_Texture* snowTexSmall;
    SDL_Texture* snowTexMedium;
    SDL_Texture* snowTexLarge;

    // Random number generator
    std::mt19937 rng;
};

#endif // SNOW_SYSTEM_H
