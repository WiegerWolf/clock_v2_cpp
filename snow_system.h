// snow_system.h
#ifndef SNOW_SYSTEM_H
#define SNOW_SYSTEM_H

#include <SDL.h>
#include <vector>

struct Snowflake {
    float x, y, speed, drift;
    int radius;
};

class SnowSystem {
public:
    SnowSystem(int numFlakes, int screenWidth, int screenHeight);
    ~SnowSystem() = default;

    void update(double wind);
    void draw(SDL_Renderer* renderer);

private:
    Snowflake createSnowflake(int screenWidth, int screenHeight);
    std::vector<Snowflake> snowflakes;
    int screenWidth, screenHeight;
};

#endif // SNOW_SYSTEM_H