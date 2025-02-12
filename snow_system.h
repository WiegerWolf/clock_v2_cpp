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
    ~SnowSystem();

    void initialize(SDL_Renderer* renderer);
    void update(double wind, const Display* display);
    void draw(SDL_Renderer* renderer);

private:
    Snowflake createSnowflake(int screenWidth, int screenHeight);
    void updateVertexBuffers();
    void initializeVertexBuffers();

    std::vector<Snowflake> snowflakes;
    int screenWidth, screenHeight;
    SDL_Renderer* renderer;

    // Batched rendering data
    static const int MAX_BATCH_SIZE = 1024;
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    SDL_Texture* snowTextures[3];  // One texture per size
    
    // Vertex buffers for each snowflake size
    struct BatchGroup {
        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
        size_t count;
    };
    BatchGroup batchGroups[3];  // One group per size

    static const int SETTLE_TIMEOUT = 1000;
    static constexpr float CLOCK_PLANE_DEPTH = 0.0f;
    static constexpr float DEPTH_COLLISION_THRESHOLD = 0.1f;
};

#endif // SNOW_SYSTEM_H