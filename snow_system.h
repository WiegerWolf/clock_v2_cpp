#ifndef SNOW_SYSTEM_H
#define SNOW_SYSTEM_H

#include <SDL2/SDL.h>
#include <vector>
#include <random>

struct Snowflake {
    float x, y;
    float speed;
    float drift;
    float angle;
    float angleVel;
    int radius;
    float depth;
};

class SnowSystem {
public:
    SnowSystem(int flakeCount, int screenWidth, int screenHeight);
    ~SnowSystem();

    void initialize(SDL_Renderer* renderer);
    void update();
    void draw(SDL_Renderer* renderer);

private:
    // Configuration
    int numFlakes;
    int screenWidth;
    int screenHeight;

    // Rendering resources
    SDL_Renderer* renderer;
    SDL_Texture* snowTexSmall;
    SDL_Texture* snowTexMedium;
    SDL_Texture* snowTexLarge;

    // Snowflake data
    std::vector<Snowflake> snowflakes;
    std::mt19937 rng;

    // Helper functions
    SDL_Texture* createCircleTexture(int radius, Uint8 alpha);
    Snowflake createSnowflake();
    void resetSnowflake(Snowflake& snow);
};

#endif // SNOW_SYSTEM_H