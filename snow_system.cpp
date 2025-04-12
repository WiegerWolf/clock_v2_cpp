// snow_system.cpp
#include "snow_system.h"
#include "config.h" // Keep for screen dimensions if needed, or remove if passed in
#include <random>
#include <algorithm>
#include <iostream>
#include <vector> // Ensure vector is included
#include <cmath> // For fmod, clamp
#include <algorithm> // For std::sort, std::clamp

// Helper to create snowflake textures (used during initialization)
// Moved inside the class in the header, or keep as static helper if preferred
// For simplicity, let's make it a private member function or keep it static here.
// Keeping it static here for minimal changes to the call sites.
namespace {
    // Static helper remains valid
    static SDL_Texture* createCircleTexture(SDL_Renderer* renderer, int radius, Uint8 alpha = 255) {
        const int diameter = radius * 2 + 2; // Add padding for smoother edges
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, diameter, diameter, 32, SDL_PIXELFORMAT_RGBA32
        );
        
        // Removed duplicate line: 0, diameter, diameter, 32, SDL_PIXELFORMAT_RGBA32

        if (!surface) {
            std::cerr << "Failed to create surface for snow texture: " << SDL_GetError() << std::endl;
            return nullptr;
        }

        // Make surface transparent
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
        SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

        // Draw circle - Restore original alpha blending logic
        Sint16 centerX = radius + 1;
        Sint16 centerY = radius + 1;
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x*x + y*y <= radius*radius) {
                    // Draw solid white pixel with the provided base alpha
                    Uint8 pixelAlpha = alpha; // Use the alpha passed to the function (or 255 for fully opaque)

                    Uint32* target_pixel = static_cast<Uint32*>(surface->pixels) + (centerY + y) * surface->pitch / 4 + (centerX + x);
                    *target_pixel = SDL_MapRGBA(surface->format, 255, 255, 255, pixelAlpha);
                }
            }
        }
        // End original alpha blending logic

        // SDL_UnlockSurface(surface); // Not needed for software surface manipulation

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface); // Free the surface after creating texture

        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        }

        return texture;
    } // End of createCircleTexture function
} // End of anonymous namespace

SnowSystem::SnowSystem(int flakes, int width, int height)
    : numFlakes(flakes), screenWidth(width), screenHeight(height), renderer(nullptr),
      snowTexSmall(nullptr), snowTexMedium(nullptr), snowTexLarge(nullptr),
      rng(std::random_device{}()) // Initialize RNG
{
    snowflakes.reserve(numFlakes);
}

SnowSystem::~SnowSystem() {
    // Clean up base textures
    if (snowTexSmall) SDL_DestroyTexture(snowTexSmall);
    if (snowTexMedium) SDL_DestroyTexture(snowTexMedium);
    if (snowTexLarge) SDL_DestroyTexture(snowTexLarge);
    snowflakes.clear();
}

// Create individual snowflake data
Snowflake SnowSystem::createSnowflake(int width, int height) {
    // Use member rng now
    std::uniform_real_distribution<float> distrib_pos_x(-50.0f, static_cast<float>(width) + 50.0f);
    std::uniform_real_distribution<float> distrib_pos_y(-50.0f, static_cast<float>(height) + 50.0f);
    // Speed and drift no longer need scaling by PRE_RENDER_FPS
    std::uniform_real_distribution<float> distrib_speed(0.5f, 1.5f); // Adjust speed range as needed
    std::uniform_real_distribution<float> distrib_drift(-0.1f, 0.1f); // Adjust drift range as needed
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-0.5f, 0.5f); // Adjust angular velocity as needed
    std::uniform_int_distribution<> distrib_radius(2, 4);
    std::uniform_real_distribution<float> distrib_depth(-1.0f, 1.0f);

    return {
        distrib_pos_x(rng), // Use member rng
        distrib_pos_y(rng),
        distrib_speed(rng),
        distrib_drift(rng),
        distrib_angle(rng),
        distrib_angle_vel(rng),
        distrib_radius(rng),
        distrib_depth(rng)
    };
}

// Initialize snowflake particles and base textures
void SnowSystem::initialize(SDL_Renderer* r) {
    renderer = r;
    if (!renderer) {
        std::cerr << "Invalid renderer provided to snow system" << std::endl;
        return;
    }

    std::cout << "Initializing snow..." << std::endl;

    // 1. Create base snowflake textures
    // Use the static helper function defined above
    snowTexSmall = createCircleTexture(renderer, 2, 200);
    snowTexMedium = createCircleTexture(renderer, 3, 220);
    snowTexLarge = createCircleTexture(renderer, 4, 240);

    if (!snowTexSmall || !snowTexMedium || !snowTexLarge) {
        std::cerr << "Failed to create base snowflake textures." << std::endl;
        // Destructor will clean up any textures that were created successfully
        return;
    }

    // 2. Create initial snowflake particles
    snowflakes.clear(); // Ensure vector is empty
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }

    std::cout << "Snow initialization complete." << std::endl;
}

// Update snowflake physics
void SnowSystem::update() {
    // Use member rng
    std::uniform_real_distribution<float> distrib_drift_rand(-0.02f, 0.02f); // Random drift change
    // double wind = 0.0; // Add wind factor here if needed

    std::vector<Snowflake> nextFrameSnowflakes;
    nextFrameSnowflakes.reserve(snowflakes.size());

    for (auto& snow : snowflakes) {
        snow.y += snow.speed;

        float drift_change = distrib_drift_rand(rng); // Use member rng
        snow.drift = std::clamp(snow.drift + drift_change, -0.5f, 0.5f); // Clamp drift range
        snow.x += snow.drift; // + wind * (snow.radius / 3.0f); // Add wind effect if needed

        // Wrap around screen edges (allow going further off-screen before reset)
        if (snow.x < -snow.radius * 2 - 50) snow.x = screenWidth + snow.radius * 2 + 50;
        else if (snow.x > screenWidth + snow.radius * 2 + 50) snow.x = -snow.radius * 2 - 50;

        // Reset snowflake if it goes too far below screen
        if (snow.y > screenHeight + snow.radius * 2 + 50) {
            nextFrameSnowflakes.push_back(createSnowflake(screenWidth, screenHeight));
            nextFrameSnowflakes.back().y = -snow.radius * 2.0f - 50.0f; // Start above screen
        } else {
            snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);
            if (snow.angle < 0) snow.angle += 360.0f; // Ensure angle stays positive
            nextFrameSnowflakes.push_back(snow);
        }
    }
    snowflakes.swap(nextFrameSnowflakes); // Update snowflake list
}

// Draw snowflakes directly using base textures
void SnowSystem::draw(SDL_Renderer* renderer) {
    if (!renderer || snowflakes.empty() || !snowTexSmall || !snowTexMedium || !snowTexLarge) {
        return; // Need renderer, snowflakes, and textures
    }

    // Sort by depth for pseudo-3D effect
    std::sort(snowflakes.begin(), snowflakes.end(),
              [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });

    // Ensure blend mode is set correctly
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Texture* snowTextures[] = { snowTexSmall, snowTexMedium, snowTexLarge };

    for (const auto& snow : snowflakes) {
        int texIndex = snow.radius - 2;
        if (texIndex < 0 || texIndex > 2) continue; // Safety check

        SDL_Texture* currentSnowTex = snowTextures[texIndex];
        int texW, texH;
        SDL_QueryTexture(currentSnowTex, NULL, NULL, &texW, &texH);

        SDL_Rect destRect = {
            static_cast<int>(snow.x - texW / 2.0f),
            static_cast<int>(snow.y - texH / 2.0f),
            texW,
            texH
        };

        // Optional: Adjust alpha based on depth or other factors if desired
        // float alphaFactor = (snow.depth + 1.0f) / 2.0f; // Example: fade based on depth
        // Uint8 alpha = static_cast<Uint8>(alphaFactor * 255);
        // SDL_SetTextureAlphaMod(currentSnowTex, alpha);

        // Reset color mod (base textures are already white)
        SDL_SetTextureColorMod(currentSnowTex, 255, 255, 255);
        // Use the alpha baked into the base texture (or set explicitly if needed)
        // SDL_SetTextureAlphaMod(currentSnowTex, 255); // Assuming base textures have alpha

        SDL_RenderCopyEx(renderer, currentSnowTex, nullptr, &destRect, snow.angle, nullptr, SDL_FLIP_NONE);
    }
}
