// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>
#include <algorithm>
#include <array>
#include <iostream>  // Added for diagnostic output

namespace {
    std::mt19937 rng{std::random_device{}()};
    
    SDL_Texture* createCircleTexture(SDL_Renderer* renderer, int radius) {
        const int diameter = radius * 2;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, diameter, diameter, 32, SDL_PIXELFORMAT_RGBA32
        );
        
        if (!surface) {
            std::cerr << "Failed to create surface for snow texture" << std::endl;
            return nullptr;
        }

        SDL_LockSurface(surface);
        Uint32* pixels = static_cast<Uint32*>(surface->pixels);
        
        // Draw a smooth circle with alpha gradient
        for (int y = 0; y < diameter; y++) {
            for (int x = 0; x < diameter; x++) {
                float dx = x - radius;
                float dy = y - radius;
                float distance = std::sqrt(dx*dx + dy*dy);
                
                if (distance <= radius) {
                    float alpha = 1.0f - (distance / radius);
                    alpha = alpha * alpha; // Square it for smoother falloff
                    Uint8 a = static_cast<Uint8>(alpha * 255);
                    pixels[y * diameter + x] = (a << 24) | 0xFFFFFF;
                } else {
                    pixels[y * diameter + x] = 0;
                }
            }
        }
        
        SDL_UnlockSurface(surface);
        
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);  // Free the surface after creating texture
        
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        }
        
        return texture;
    }
}

SnowSystem::SnowSystem(int numFlakes, int width, int height) 
    : screenWidth(width), screenHeight(height), renderer(nullptr) {
    
    snowflakes.reserve(numFlakes);
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }
    
    // Initialize batch groups
    for (int i = 0; i < 3; ++i) {
        batchGroups[i].vertices.reserve(MAX_BATCH_SIZE * 4);  // 4 vertices per snowflake
        batchGroups[i].indices.reserve(MAX_BATCH_SIZE * 6);   // 6 indices per snowflake
        batchGroups[i].count = 0;
        snowTextures[i] = nullptr;  // Initialize texture pointers to nullptr
    }
}

void SnowSystem::initialize(SDL_Renderer* r) {
    renderer = r;
    if (!renderer) {
        std::cerr << "Invalid renderer provided to snow system" << std::endl;
        return;
    }

    bool texturesCreated = true;
    
    // Create optimized textures for each size
    for (int i = 0; i < 3; ++i) {
        snowTextures[i] = createCircleTexture(renderer, i + 2);  // Radius 2-4 instead of 1-3
        if (!snowTextures[i]) {
            std::cerr << "Failed to create snow texture " << i << ": " << SDL_GetError() << std::endl;
            texturesCreated = false;
            break;  // Exit if texture creation fails
        }
    }
    
    if (!texturesCreated) {
        // Clean up any textures that were created
        for (int i = 0; i < 3; ++i) {
            if (snowTextures[i]) {
                SDL_DestroyTexture(snowTextures[i]);
                snowTextures[i] = nullptr;
            }
        }
        std::cerr << "Snow system initialization failed due to texture creation errors" << std::endl;
        return;
    }
    
    initializeVertexBuffers();
}

void SnowSystem::initializeVertexBuffers() {
    // Pre-compute index patterns for quads
    for (int i = 0; i < 3; i++) {
        auto& group = batchGroups[i];
        group.indices.clear();
        
        // Make sure we have enough space reserved
        group.indices.reserve(MAX_BATCH_SIZE * 6);  // 6 indices per snowflake
        
        for (int j = 0; j < MAX_BATCH_SIZE; ++j) {
            int baseVertex = j * 4;
            // Add indices for two triangles forming a quad
            group.indices.push_back(baseVertex);
            group.indices.push_back(baseVertex + 1);
            group.indices.push_back(baseVertex + 2);
            group.indices.push_back(baseVertex + 2);
            group.indices.push_back(baseVertex + 3);
            group.indices.push_back(baseVertex);
        }
        
        // Reset count
        group.count = 0;
    }
}

SnowSystem::~SnowSystem() {
    // First clear all batch groups to ensure no references to textures remain
    for (int i = 0; i < 3; i++) {
        batchGroups[i].vertices.clear();
        batchGroups[i].indices.clear();
        batchGroups[i].count = 0;
    }
    
    // Now safely destroy textures
    for (int i = 0; i < 3; i++) {
        if (snowTextures[i]) {
            SDL_DestroyTexture(snowTextures[i]);
            snowTextures[i] = nullptr;
        }
    }
    
    // Clear snowflakes vector
    snowflakes.clear();
}

Snowflake SnowSystem::createSnowflake(int width, int height) {
    std::uniform_real_distribution<float> distrib_pos_x(0.0f, static_cast<float>(width));
    std::uniform_real_distribution<float> distrib_pos_y(0.0f, static_cast<float>(height));
    std::uniform_real_distribution<float> distrib_speed(0.3f, 1.2f);
    std::uniform_real_distribution<float> distrib_drift(-0.2f, 0.2f);
    std::uniform_real_distribution<float> distrib_alpha(0.4f, 0.9f);  // Increased opacity range
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-1.0f, 1.0f);
    std::uniform_int_distribution<> distrib_radius(2, 4);  // Radius range must match texture creation
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

void SnowSystem::update(double wind, const Display* display) {
    if (!display) return;  // Safety check
    
    std::uniform_real_distribution<float> distrib_drift_rand(-0.05f, 0.05f);
    bool textureChanged = display->hasTextureChanged();
    
    std::vector<Snowflake> newSnowflakes;  // Temporary vector for new snowflakes
    newSnowflakes.reserve(snowflakes.size());  // Reserve space to avoid reallocations
    
    // Update in chunks for better cache utilization
    for (auto& snow : snowflakes) {
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
                newSnowflakes.push_back(createSnowflake(screenWidth, screenHeight));
                newSnowflakes.back().y = -10;
                continue;
            }
            newSnowflakes.push_back(snow);
            continue;
        }

        snow.y += snow.speed * 0.7f;
        
        float drift_change = distrib_drift_rand(rng);
        snow.drift = std::clamp(snow.drift + drift_change, -1.0f, 1.0f);
        snow.x += snow.drift + (static_cast<float>(wind) * (snow.radius / 3.0f));

        // Wrap around screen edges
        if (snow.x < -10) snow.x = screenWidth + 10;
        else if (snow.x > screenWidth + 10) snow.x = -10;

        // Reset snowflake if it goes off screen
        if (snow.y > screenHeight + 10) {
            newSnowflakes.push_back(createSnowflake(screenWidth, screenHeight));
            newSnowflakes.back().y = -10;
        } else {
            snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);
            newSnowflakes.push_back(snow);
        }
    }
    
    // Safely swap the vectors
    snowflakes.swap(newSnowflakes);
    
    // Update vertex buffers after physics update
    updateVertexBuffers();
}

void SnowSystem::updateVertexBuffers() {
    // Clear batch groups
    for (int i = 0; i < 3; ++i) {
        batchGroups[i].vertices.clear();
        batchGroups[i].count = 0;
    }
    
    // Sort snowflakes by depth for correct rendering
    std::sort(snowflakes.begin(), snowflakes.end(),
              [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });
    
    // Update vertex buffers for each snowflake
    for (const auto& snow : snowflakes) {
        // Adjust batchIndex for new radius range (2-4)
        int batchIndex = snow.radius - 2;
        if (batchIndex < 0 || batchIndex >= 3) continue; // Skip invalid sizes
        
        auto& group = batchGroups[batchIndex];
        if (group.count >= MAX_BATCH_SIZE) continue;
        
        float size = snow.radius * 2.0f;
        // Simple alpha mapping
        Uint8 alpha = static_cast<Uint8>(snow.alpha * 255);
        SDL_Color color = {255, 255, 255, alpha};
        
        // Calculate vertex positions
        SDL_Vertex vertices[4] = {
            {{snow.x - size, snow.y - size}, color, {0.0f, 0.0f}},
            {{snow.x + size, snow.y - size}, color, {1.0f, 0.0f}},
            {{snow.x + size, snow.y + size}, color, {1.0f, 1.0f}},
            {{snow.x - size, snow.y + size}, color, {0.0f, 1.0f}}
        };
        
        // Make sure we have enough space reserved
        if (group.vertices.size() + 4 > group.vertices.capacity()) {
            group.vertices.reserve(group.vertices.capacity() * 2);
        }
        
        group.vertices.insert(group.vertices.end(), vertices, vertices + 4);
        group.count++;
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    if (!renderer) return;
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Render each batch group
    for (int i = 0; i < 3; ++i) {
        if (!snowTextures[i]) continue;  // Skip if texture wasn't created successfully
        
        const auto& group = batchGroups[i];
        if (group.count == 0) continue;
        
        // Ensure we have valid data to render and the right number of indices
        if (group.vertices.empty() || 
            group.indices.empty() || 
            group.indices.size() < group.count * 6) {
            std::cerr << "Invalid batch data for group " << i << std::endl;
            continue;
        }
        
        // Verify vertex count matches index requirements
        if (group.vertices.size() < group.count * 4) {
            std::cerr << "Insufficient vertices for batch " << i << std::endl;
            continue;
        }
        
        SDL_RenderGeometry(renderer,
                          snowTextures[i],
                          group.vertices.data(),
                          group.vertices.size(),
                          group.indices.data(),
                          group.count * 6);
    }
}