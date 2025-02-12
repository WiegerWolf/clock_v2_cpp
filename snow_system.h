// snow_system.h
#ifndef SNOW_SYSTEM_H
#define SNOW_SYSTEM_H

#include <SDL.h>
#include <vector>
#include "display.h"

struct Snowflake {
    float x, y, speed, drift;
    float alpha;        // Opacity
    float angle;        // Current rotation angle
    float angleVel;     // Angular velocity
    int radius;
    bool settled;       // Whether the snowflake has settled on text
    int settleTime;     // How long it's been settled
    float depth;        // Z-position: -1.0 (behind) to 1.0 (in front)
};

class SnowSystem {
public:
    SnowSystem(int numFlakes, int screenWidth, int screenHeight);
    ~SnowSystem();  // Changed from default to handle texture cleanup

    void initialize(SDL_Renderer* renderer);  // New method to initialize textures
    void update(double wind, const Display* display);
    void draw(SDL_Renderer* renderer);

private:
    Snowflake createSnowflake(int screenWidth, int screenHeight);
    std::vector<Snowflake> snowflakes;
    int screenWidth, screenHeight;
    SDL_Renderer* renderer;  // Store renderer for texture management

    static const int SETTLE_TIMEOUT = 1000; // Time before settled snow disappears
    static constexpr float CLOCK_PLANE_DEPTH = 0.0f;
    static constexpr float DEPTH_COLLISION_THRESHOLD = 0.1f;  // How close to clock plane to check collisions
};

#endif // SNOW_SYSTEM_H