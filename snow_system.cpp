// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>

Snowflake SnowSystem::createSnowflake(int width, int height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distrib_pos_x(0.0f, static_cast<float>(width));
    std::uniform_real_distribution<float> distrib_pos_y(0.0f, static_cast<float>(height));
    std::uniform_real_distribution<float> distrib_speed(1.0f, 3.0f);
    std::uniform_real_distribution<float> distrib_drift(-0.5f, 0.5f);
    std::uniform_real_distribution<float> distrib_alpha(0.3f, 0.9f);
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-2.0f, 2.0f);
    std::uniform_int_distribution<> distrib_radius(1, 4);

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
        0      // settleTime
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
    std::uniform_real_distribution<float> distrib_drift_rand(-0.1f, 0.1f);

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
        snow.y += snow.speed;
        float new_drift = snow.drift + distrib_drift_rand(gen);
        snow.drift = std::max(-2.0f, std::min(2.0f, new_drift));  // Use consistent float types
        snow.x += snow.drift + (static_cast<float>(wind) * (snow.radius / 2.0f));  // Larger flakes are more affected by wind

        // Check for collision with text
        int currentX = static_cast<int>(snow.x);
        int currentY = static_cast<int>(snow.y);
        
        // Check a few pixels below the snowflake for collision
        for (int checkY = prevY; checkY <= currentY; ++checkY) {
            if (display->isPixelOccupied(currentX, checkY + snow.radius)) {
                snow.y = checkY;
                snow.settled = true;
                snow.settleTime = 0;
                break;
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
    for (const auto& snow : snowflakes) {
        // Set color with alpha
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(snow.alpha * 255));

        // Draw a filled circle for each snowflake
        const int32_t diameter = snow.radius * 2;
        const int32_t radius = snow.radius;
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