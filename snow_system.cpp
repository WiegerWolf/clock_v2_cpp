// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>
#include <algorithm>  // Add this for std::sort

Snowflake SnowSystem::createSnowflake(int width, int height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distrib_pos_x(0.0f, static_cast<float>(width));
    std::uniform_real_distribution<float> distrib_pos_y(0.0f, static_cast<float>(height));
    std::uniform_real_distribution<float> distrib_speed(0.3f, 1.2f);     // Reduced speed range
    std::uniform_real_distribution<float> distrib_drift(-0.2f, 0.2f);    // Reduced drift range
    std::uniform_real_distribution<float> distrib_alpha(0.2f, 0.7f);     // Reduced opacity range
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-1.0f, 1.0f); // Reduced rotation speed
    std::uniform_int_distribution<> distrib_radius(1, 3);                 // Smaller snowflakes
    std::uniform_real_distribution<float> distrib_depth(-1.0f, 1.0f);

    return {
        distrib_pos_x(gen),
        distrib_pos_y(gen),  // Random initial y position
        distrib_speed(gen),
        distrib_drift(gen),
        distrib_alpha(gen),
        distrib_angle(gen),
        distrib_angle_vel(gen),
        distrib_radius(gen),
        false,  // settled
         0,     // settleTime
        distrib_depth(gen)  // Random depth
    };
}

SnowSystem::SnowSystem(int numFlakes, int width, int height) : screenWidth(width), screenHeight(height) {
    snowflakes.reserve(numFlakes);
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }
}

void SnowSystem::update(double wind, const Display* display) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distrib_drift_rand(-0.05f, 0.05f);  // Reduced random drift

    // Check if texture changed and unfreeze affected snowflakes
    bool textureChanged = display->hasTextureChanged();

    for (auto& snow : snowflakes) {
        // If texture changed and snowflake is settled, check if it should be unfrozen
        if (textureChanged && snow.settled) {
            int currentX = static_cast<int>(snow.x);
            int currentY = static_cast<int>(snow.y);
            if (!display->isPixelOccupied(currentX, currentY + snow.radius)) {
                snow.settled = false;
                snow.settleTime = 0;
            }
        }

        if (snow.settled) {
            snow.settleTime++;
            if (snow.settleTime > SETTLE_TIMEOUT) {
                // Make the snowflake disappear and create a new one
                snow = createSnowflake(screenWidth, screenHeight);
                snow.y = -10;
            }
            continue;
        }

        // Previous position for collision check
        int prevY = static_cast<int>(snow.y);
        
        // Update position
        snow.y += snow.speed * 0.7f;  // Slow down vertical movement
        float new_drift = snow.drift + distrib_drift_rand(gen);
        snow.drift = std::max(-1.0f, std::min(1.0f, new_drift));  // Limit drift range
        snow.x += snow.drift + (static_cast<float>(wind) * (snow.radius / 3.0f));  // Reduced wind effect

        // Only check for collisions if snowflake is near the clock plane
        bool nearClockPlane = std::abs(snow.depth - CLOCK_PLANE_DEPTH) < DEPTH_COLLISION_THRESHOLD;
        
        if (nearClockPlane) {
            // Check for collision with text
            int currentX = static_cast<int>(snow.x);
            int currentY = static_cast<int>(snow.y);
            
            // Check collision only for snowflakes near the clock plane
            for (int checkY = prevY; checkY <= currentY; ++checkY) {
                if (display->isPixelOccupied(currentX, checkY + snow.radius)) {
                    snow.y = checkY;
                    snow.settled = true;
                    snow.settleTime = 0;
                    snow.depth = CLOCK_PLANE_DEPTH;  // Snap to clock plane when settled
                    break;
                }
            }
        }

        // Update rotation
        snow.angle += snow.angleVel;
        if (snow.angle > 360.0f) snow.angle -= 360.0f;

        // Screen wrapping
        if (snow.x < -10) snow.x = screenWidth + 10;
        if (snow.x > screenWidth + 10) snow.x = -10;

        // Reset when below screen
        if (snow.y > screenHeight + 10) {
            snow = createSnowflake(screenWidth, screenHeight);
            snow.y = -10; // Start slightly above screen
        }
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    // Sort snowflakes by depth for proper rendering
    auto snowflakesCopy = snowflakes;  // Create a copy for sorting
    std::sort(snowflakesCopy.begin(), snowflakesCopy.end(), 
              [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });

    for (const auto& snow : snowflakesCopy) {
        // Adjust size and opacity based on depth with softer scaling
        float depthFactor = (snow.depth + 1.0f) * 0.5f;
        int scaledRadius = static_cast<int>(snow.radius * (0.7f + depthFactor * 0.5f));  // Less dramatic size variation
        Uint8 alpha = static_cast<Uint8>((0.4f + 0.4f * depthFactor) * snow.alpha * 255);  // Softer opacity range

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);

        // Draw a filled circle for each snowflake
        const int32_t diameter = scaledRadius * 2;
        const int32_t radius = scaledRadius;
        const int32_t centreX = static_cast<int32_t>(snow.x);
        const int32_t centreY = static_cast<int32_t>(snow.y);

        for (int32_t w = 0; w < diameter; w++) {
            for (int32_t h = 0; h < diameter; h++) {
                int32_t dx = radius - w;
                int32_t dy = radius - h;
                if ((dx*dx + dy*dy) <= (radius * radius)) {
                    SDL_RenderDrawPoint(renderer, centreX + dx, centreY + dy);
                }
            }
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}