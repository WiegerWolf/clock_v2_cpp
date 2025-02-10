// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>
#include <algorithm>
#include <vector>

// Pre-calculate circle points for snowflake rendering
struct CirclePoints {
    std::vector<SDL_Point> points;
    int radius;
};
std::vector<CirclePoints> preCalculatedCircles;

void initializeCirclePoints() {
    if (!preCalculatedCircles.empty()) return;
    
    // Pre-calculate points for radii 1-3
    for (int radius = 1; radius <= 3; radius++) {
        CirclePoints cp;
        cp.radius = radius;
        const int32_t diameter = radius * 2;

        for (int32_t w = 0; w < diameter; w++) {
            for (int32_t h = 0; h < diameter; h++) {
                int32_t dx = radius - w;
                int32_t dy = radius - h;
                if ((dx*dx + dy*dy) <= (radius * radius)) {
                    cp.points.push_back({dx, dy});
                }
            }
        }
        preCalculatedCircles.push_back(cp);
    }
}

Snowflake SnowSystem::createSnowflake(int width, int height) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> distrib_pos_x(0.0f, static_cast<float>(width));
    static std::uniform_real_distribution<float> distrib_pos_y(0.0f, static_cast<float>(height));
    static std::uniform_real_distribution<float> distrib_speed(0.3f, 1.2f);
    static std::uniform_real_distribution<float> distrib_drift(-0.2f, 0.2f);
    static std::uniform_real_distribution<float> distrib_alpha(0.2f, 0.7f);
    static std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    static std::uniform_real_distribution<float> distrib_angle_vel(-1.0f, 1.0f);
    static std::uniform_int_distribution<> distrib_radius(1, 3);
    static std::uniform_real_distribution<float> distrib_depth(-1.0f, 1.0f);

    return {
        distrib_pos_x(gen),
        distrib_pos_y(gen),
        distrib_speed(gen),
        distrib_drift(gen),
        distrib_alpha(gen),
        distrib_angle(gen),
        distrib_angle_vel(gen),
        distrib_radius(gen),
        false,
        0,
        distrib_depth(gen)
    };
}

SnowSystem::SnowSystem(int numFlakes, int width, int height) : screenWidth(width), screenHeight(height) {
    initializeCirclePoints();
    snowflakes.reserve(numFlakes);
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }

    // Initialize render targets
    batchedPoints.reserve(numFlakes * 9); // Maximum points per snowflake
}

void SnowSystem::update(double wind, const Display* display) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> distrib_drift_rand(-0.05f, 0.05f);
    
    const float windEffect = static_cast<float>(wind) * 0.3f;
    const bool textureChanged = display->hasTextureChanged();
    
    #pragma omp parallel for if(snowflakes.size() > 1000)
    for (size_t i = 0; i < snowflakes.size(); ++i) {
        auto& snow = snowflakes[i];
        if (textureChanged && snow.settled) {
            int currentX = static_cast<int>(snow.x);
            int currentY = static_cast<int>(snow.y);
            if (!display->isPixelOccupied(currentX, currentY + snow.radius)) {
                snow.settled = false;
                snow.settleTime = 0;
            }
            continue;
        }

        if (snow.settled) {
            snow.settleTime++;
            if (snow.settleTime > SETTLE_TIMEOUT) {
                snow = createSnowflake(screenWidth, screenHeight);
                snow.y = -10;
            }
            continue;
        }

        // Fast-path for snowflakes far from the clock plane
        if (std::abs(snow.depth - CLOCK_PLANE_DEPTH) > DEPTH_COLLISION_THRESHOLD) {
            snow.y += snow.speed * 0.7f;
            snow.x += snow.drift + windEffect;
            snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);
        } else {
            int prevY = static_cast<int>(snow.y);
            snow.y += snow.speed * 0.7f;
            snow.drift = std::clamp(snow.drift + distrib_drift_rand(gen), -1.0f, 1.0f);
            snow.x += snow.drift + (windEffect * snow.radius);
            
            int currentX = static_cast<int>(snow.x);
            int currentY = static_cast<int>(snow.y);
            
            // Use a larger step for collision checks
            for (int checkY = prevY; checkY <= currentY; checkY += snow.radius * 2) {
                if (display->isPixelOccupied(currentX, checkY + snow.radius)) {
                    snow.y = checkY;
                    snow.settled = true;
                    snow.settleTime = 0;
                    snow.depth = CLOCK_PLANE_DEPTH;
                    break;
                }
            }
            snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);
        }

        // Screen wrapping with reduced branching
        snow.x = snow.x < -10 ? screenWidth + 10 : (snow.x > screenWidth + 10 ? -10 : snow.x);
        
        if (snow.y > screenHeight + 10) {
            snow = createSnowflake(screenWidth, screenHeight);
            snow.y = -10;
        }
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    // Pre-sort snowflakes by radius to minimize state changes
    static std::vector<const Snowflake*> sortedFlakes;
    sortedFlakes.clear();
    sortedFlakes.reserve(snowflakes.size());
    
    for (const auto& flake : snowflakes) {
        sortedFlakes.push_back(&flake);
    }
    
    std::sort(sortedFlakes.begin(), sortedFlakes.end(), 
              [](const Snowflake* a, const Snowflake* b) {
                  return a->radius < b->radius || (a->radius == b->radius && a->depth < b->depth);
              });

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Batch rendering by radius
    int currentRadius = -1;
    std::vector<SDL_Point> batchPoints;
    batchPoints.reserve(sortedFlakes.size() * 9);  // Approximate max points per snowflake
    
    for (const Snowflake* snow : sortedFlakes) {
        if (snow->radius != currentRadius) {
            if (!batchPoints.empty()) {
                SDL_RenderDrawPoints(renderer, batchPoints.data(), batchPoints.size());
                batchPoints.clear();
            }
            currentRadius = snow->radius;
            const auto& circlePoints = preCalculatedCircles[currentRadius-1].points;
            
            float depthFactor = (snow->depth + 1.0f) * 0.5f;
            Uint8 alpha = static_cast<Uint8>((0.4f + 0.4f * depthFactor) * snow->alpha * 255);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
        }
        
        int centerX = static_cast<int>(snow->x);
        int centerY = static_cast<int>(snow->y);
        
        for (const auto& p : preCalculatedCircles[currentRadius-1].points) {
            batchPoints.push_back({centerX + p.x, centerY + p.y});
        }
        
        if (batchPoints.size() >= 1000) {
            SDL_RenderDrawPoints(renderer, batchPoints.data(), batchPoints.size());
            batchPoints.clear();
        }
    }
    
    if (!batchPoints.empty()) {
        SDL_RenderDrawPoints(renderer, batchPoints.data(), batchPoints.size());
    }
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}