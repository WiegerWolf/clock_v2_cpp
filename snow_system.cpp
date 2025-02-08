// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>

Snowflake SnowSystem::createSnowflake(int width, int height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distrib_pos_x(0.0f, static_cast<float>(width));
    std::uniform_real_distribution<float> distrib_speed(0.5f, 2.0f);
    std::uniform_real_distribution<float> distrib_drift(-0.2f, 0.2f);
    std::uniform_int_distribution<> distrib_radius(2, 4);

    return {
        distrib_pos_x(gen),
        0.0f,
        distrib_speed(gen),
        distrib_drift(gen),
        distrib_radius(gen)
    };
}

SnowSystem::SnowSystem(int numFlakes, int width, int height) : screenWidth(width), screenHeight(height) {
    snowflakes.reserve(numFlakes);
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }
}

void SnowSystem::update(double wind) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> distrib_drift_rand(-0.05f, 0.05f);

    for (auto& snow : snowflakes) {
        snow.y += snow.speed;
        snow.x += snow.drift + distrib_drift_rand(gen) + wind;

        if (snow.y > screenHeight) {
            snow = createSnowflake(screenWidth, screenHeight);
            snow.y = 0; // Reset y to top
        }
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, WHITE_COLOR.r, WHITE_COLOR.g, WHITE_COLOR.b, WHITE_COLOR.a);
    for (const auto& snow : snowflakes) {
        SDL_FRect rect = {snow.x - snow.radius, snow.y - snow.radius, static_cast<float>(snow.radius * 2), static_cast<float>(snow.radius * 2)};
        SDL_RenderFillRectF(renderer, &rect);
    }
}