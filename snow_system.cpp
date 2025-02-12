// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>
#include <algorithm>
#include <array>

namespace {
    // Pre-computed circle texture for different radii
    struct CircleTexture {
        SDL_Texture* texture;
        int size;
    };
    std::array<CircleTexture, 3> circleTextures;
    
    // Single RNG instance
    std::mt19937 rng{std::random_device{}()};
    
    SDL_Texture* createCircleTexture(SDL_Renderer* renderer, int radius) {
        const int diameter = radius * 2;
        SDL_Texture* texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STATIC,
            diameter, diameter
        );
        
        std::vector<uint32_t> pixels(diameter * diameter, 0);
        for (int y = 0; y < diameter; y++) {
            for (int x = 0; x < diameter; x++) {
                int dx = x - radius;
                int dy = y - radius;
                if (dx*dx + dy*dy <= radius*radius) {
                    pixels[y * diameter + x] = 0xFFFFFFFF;
                }
            }
        }
        
        SDL_UpdateTexture(texture, nullptr, pixels.data(), diameter * sizeof(uint32_t));
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        return texture;
    }
}

Snowflake SnowSystem::createSnowflake(int width, int height) {
    std::uniform_real_distribution<float> distrib_pos_x(0.0f, static_cast<float>(width));
    std::uniform_real_distribution<float> distrib_pos_y(0.0f, static_cast<float>(height));
    std::uniform_real_distribution<float> distrib_speed(0.3f, 1.2f);
    std::uniform_real_distribution<float> distrib_drift(-0.2f, 0.2f);
    std::uniform_real_distribution<float> distrib_alpha(0.2f, 0.7f);
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-1.0f, 1.0f);
    std::uniform_int_distribution<> distrib_radius(1, 3);
    std::uniform_real_distribution<float> distrib_depth(-1.0f, 1.0f);

    return {
        distrib_pos_x(rng),
        distrib_pos_y(rng),
        distrib_speed(rng),
        distrib_drift(rng),
        distrib_alpha(rng),
        distrib_angle(rng),
        distrib_angle_vel(rng),
        distrib_radius(rng),
        false,
        0,
        distrib_depth(rng)
    };
}

SnowSystem::SnowSystem(int numFlakes, int width, int height) 
    : screenWidth(width), screenHeight(height), renderer(nullptr) {
    snowflakes.reserve(numFlakes);
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }
}

void SnowSystem::initialize(SDL_Renderer* r) {
    renderer = r;
    // Create circle textures for each possible radius
    for (int i = 0; i < 3; ++i) {
        circleTextures[i] = {
            createCircleTexture(renderer, i + 1),
            (i + 1) * 2
        };
    }
}

SnowSystem::~SnowSystem() {
    for (auto& ct : circleTextures) {
        if (ct.texture) {
            SDL_DestroyTexture(ct.texture);
        }
    }
}

void SnowSystem::update(double wind, const Display* display) {
    std::uniform_real_distribution<float> distrib_drift_rand(-0.05f, 0.05f);
    bool textureChanged = display->hasTextureChanged();
    
    // Update in chunks for better cache utilization
    constexpr size_t CHUNK_SIZE = 64;
    for (size_t i = 0; i < snowflakes.size(); i += CHUNK_SIZE) {
        size_t end = std::min(i + CHUNK_SIZE, snowflakes.size());
        for (size_t j = i; j < end; ++j) {
            auto& snow = snowflakes[j];
            
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
                    snow = createSnowflake(screenWidth, screenHeight);
                    snow.y = -10;
                }
                continue;
            }

            int prevY = static_cast<int>(snow.y);
            snow.y += snow.speed * 0.7f;
            
            // Update drift with temporal coherence
            float drift_change = distrib_drift_rand(rng);
            snow.drift = std::clamp(snow.drift + drift_change, -1.0f, 1.0f);
            snow.x += snow.drift + (static_cast<float>(wind) * (snow.radius / 3.0f));

            // Optimized collision detection for particles near the clock plane
            if (std::abs(snow.depth - CLOCK_PLANE_DEPTH) < DEPTH_COLLISION_THRESHOLD) {
                int currentX = static_cast<int>(snow.x);
                int currentY = static_cast<int>(snow.y);
                
                // Binary search for collision point
                while (prevY <= currentY) {
                    int midY = (prevY + currentY) / 2;
                    if (display->isPixelOccupied(currentX, midY + snow.radius)) {
                        currentY = midY - 1;
                        snow.settled = true;
                        snow.y = static_cast<float>(midY);
                        snow.settleTime = 0;
                        snow.depth = CLOCK_PLANE_DEPTH;
                    } else {
                        prevY = midY + 1;
                    }
                }
            }

            // Simplified rotation
            snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);

            // Screen wrapping
            if (snow.x < -10) snow.x = screenWidth + 10;
            else if (snow.x > screenWidth + 10) snow.x = -10;

            // Reset when below screen
            if (snow.y > screenHeight + 10) {
                snow = createSnowflake(screenWidth, screenHeight);
                snow.y = -10;
            }
        }
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    // Sort snowflakes by depth using insertion sort for small ranges
    // This is faster than std::sort for mostly-sorted data
    for (size_t i = 1; i < snowflakes.size(); ++i) {
        Snowflake key = snowflakes[i];
        int j = i - 1;
        while (j >= 0 && snowflakes[j].depth > key.depth) {
            snowflakes[j + 1] = snowflakes[j];
            --j;
        }
        snowflakes[j + 1] = key;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Batch render snowflakes by radius to minimize texture switches
    for (int radius = 1; radius <= 3; ++radius) {
        const auto& circleTexture = circleTextures[radius - 1];
        SDL_Texture* texture = circleTexture.texture;
        
        for (const auto& snow : snowflakes) {
            if (snow.radius != radius) continue;
            
            float depthFactor = (snow.depth + 1.0f) * 0.5f;
            Uint8 alpha = static_cast<Uint8>((0.4f + 0.4f * depthFactor) * snow.alpha * 255);
            SDL_SetTextureAlphaMod(texture, alpha);
            
            SDL_Rect dstRect{
                static_cast<int>(snow.x - radius),
                static_cast<int>(snow.y - radius),
                circleTexture.size,
                circleTexture.size
            };
            
            SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
        }
    }
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}