#include "snow_system.h"
#include <iostream>
#include <algorithm>
#include <cmath>

SnowSystem::SnowSystem(int flakeCount, int screenWidth, int screenHeight)
    : numFlakes(flakeCount)
    , screenWidth(screenWidth)
    , screenHeight(screenHeight)
    , renderer(nullptr)
    , snowTexSmall(nullptr)
    , snowTexMedium(nullptr)
    , snowTexLarge(nullptr)
    , rng(std::random_device{}())
{
    snowflakes.reserve(numFlakes);
}

SnowSystem::~SnowSystem() {
    if (snowTexSmall) SDL_DestroyTexture(snowTexSmall);
    if (snowTexMedium) SDL_DestroyTexture(snowTexMedium);
    if (snowTexLarge) SDL_DestroyTexture(snowTexLarge);
}

SDL_Texture* SnowSystem::createCircleTexture(int radius, Uint8 alpha) {
    const int diameter = radius * 2 + 2;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, diameter, diameter, 32, SDL_PIXELFORMAT_RGBA32
    );
    
    if (!surface) {
        std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Clear to transparent
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    // Draw filled circle
    int centerX = radius + 1;
    int centerY = radius + 1;
    int radiusSquared = radius * radius;

    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radiusSquared) {
                Uint32* pixel = static_cast<Uint32*>(surface->pixels) + 
                                (centerY + y) * (surface->pitch / 4) + (centerX + x);
                *pixel = SDL_MapRGBA(surface->format, 255, 255, 255, alpha);
            }
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    } else {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
    }

    return texture;
}

Snowflake SnowSystem::createSnowflake() {
    std::uniform_real_distribution<float> xDist(-50.0f, screenWidth + 50.0f);
    std::uniform_real_distribution<float> yDist(-50.0f, screenHeight + 50.0f);
    std::uniform_real_distribution<float> speedDist(0.5f, 1.5f);
    std::uniform_real_distribution<float> driftDist(-0.1f, 0.1f);
    std::uniform_real_distribution<float> angleDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> angleVelDist(-0.5f, 0.5f);
    std::uniform_int_distribution<int> radiusDist(2, 4);
    std::uniform_real_distribution<float> depthDist(-1.0f, 1.0f);

    return Snowflake{
        xDist(rng),
        yDist(rng),
        speedDist(rng),
        driftDist(rng),
        angleDist(rng),
        angleVelDist(rng),
        radiusDist(rng),
        depthDist(rng)
    };
}

void SnowSystem::resetSnowflake(Snowflake& snow) {
    std::uniform_real_distribution<float> xDist(-50.0f, screenWidth + 50.0f);
    
    snow.x = xDist(rng);
    snow.y = -snow.radius * 2.0f - 50.0f;
}

void SnowSystem::initialize(SDL_Renderer* r) {
    renderer = r;
    if (!renderer) {
        std::cerr << "Invalid renderer provided" << std::endl;
        return;
    }

    // Create base textures
    snowTexSmall = createCircleTexture(2, 200);
    snowTexMedium = createCircleTexture(3, 220);
    snowTexLarge = createCircleTexture(4, 240);

    if (!snowTexSmall || !snowTexMedium || !snowTexLarge) {
        std::cerr << "Failed to create snowflake textures" << std::endl;
        return;
    }

    // Create snowflakes
    snowflakes.clear();
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake());
    }

    std::cout << "Snow system initialized with " << numFlakes << " flakes" << std::endl;
}

void SnowSystem::update() {
    std::uniform_real_distribution<float> driftChangeDist(-0.02f, 0.02f);

    for (auto& snow : snowflakes) {
        // Apply gravity
        snow.y += snow.speed;

        // Update horizontal drift
        snow.drift = std::clamp(snow.drift + driftChangeDist(rng), -0.5f, 0.5f);
        snow.x += snow.drift;

        // Wrap horizontally
        float boundary = snow.radius * 2.0f + 50.0f;
        if (snow.x < -boundary) {
            snow.x = screenWidth + boundary;
        } else if (snow.x > screenWidth + boundary) {
            snow.x = -boundary;
        }

        // Reset if off bottom of screen
        if (snow.y > screenHeight + boundary) {
            resetSnowflake(snow);
        }

        // Rotate
        snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);
        if (snow.angle < 0.0f) {
            snow.angle += 360.0f;
        }
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    if (!renderer || snowflakes.empty()) {
        return;
    }

    SDL_Texture* textures[] = { snowTexSmall, snowTexMedium, snowTexLarge };

    for (const auto& snow : snowflakes) {
        int texIndex = snow.radius - 2;
        if (texIndex < 0 || texIndex > 2) continue;

        SDL_Texture* texture = textures[texIndex];
        int texW, texH;
        SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);

        SDL_Rect destRect = {
            static_cast<int>(snow.x - texW / 2.0f),
            static_cast<int>(snow.y - texH / 2.0f),
            texW,
            texH
        };

        SDL_RenderCopyEx(renderer, texture, nullptr, &destRect, 
                        snow.angle, nullptr, SDL_FLIP_NONE);
    }
}