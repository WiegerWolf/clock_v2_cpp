// snow_system.cpp
#include "snow_system.h"
#include "config.h"
#include <random>
#include <algorithm>
#include <array>

namespace {
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
        const float radiusSquared = radius * radius;
        
        // Create smooth circle with antialiasing
        for (int y = 0; y < diameter; y++) {
            for (int x = 0; x < diameter; x++) {
                float dx = x - radius + 0.5f;
                float dy = y - radius + 0.5f;
                float distSquared = dx*dx + dy*dy;
                
                if (distSquared <= radiusSquared) {
                    float alpha = 1.0f;
                    if (distSquared >= (radiusSquared - 1)) {
                        alpha = radiusSquared - distSquared;
                    }
                    uint32_t a = static_cast<uint32_t>(alpha * 255) & 0xFF;
                    pixels[y * diameter + x] = (a << 24) | 0xFFFFFF;
                }
            }
        }
        
        SDL_UpdateTexture(texture, nullptr, pixels.data(), diameter * sizeof(uint32_t));
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
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
    }
}

void SnowSystem::initialize(SDL_Renderer* r) {
    renderer = r;
    // Create optimized textures for each size
    for (int i = 0; i < 3; ++i) {
        snowTextures[i] = createCircleTexture(renderer, i + 1);
    }
    initializeVertexBuffers();
}

void SnowSystem::initializeVertexBuffers() {
    // Pre-compute index patterns for quads
    for (int i = 0; i < 3; ++i) {
        auto& group = batchGroups[i];
        group.indices.clear();
        for (int j = 0; j < MAX_BATCH_SIZE; ++j) {
            int baseVertex = j * 4;
            group.indices.push_back(baseVertex);
            group.indices.push_back(baseVertex + 1);
            group.indices.push_back(baseVertex + 2);
            group.indices.push_back(baseVertex + 2);
            group.indices.push_back(baseVertex + 3);
            group.indices.push_back(baseVertex);
        }
    }
}

SnowSystem::~SnowSystem() {
    for (int i = 0; i < 3; ++i) {
        if (snowTextures[i]) {
            SDL_DestroyTexture(snowTextures[i]);
        }
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

void SnowSystem::update(double wind, const Display* display) {
    static int frameCount = 0;
    static constexpr int SORT_INTERVAL = 30; // Sort every 30 frames
    static constexpr int COLLISION_CHECK_INTERVAL = 5; // Check collisions every 5 frames
    
    std::uniform_real_distribution<float> distrib_drift_rand(-0.05f, 0.05f);
    bool textureChanged = display->hasTextureChanged();
    bool checkCollisions = (frameCount % COLLISION_CHECK_INTERVAL) == 0;
    
    // Update in chunks for better cache utilization
    constexpr size_t CHUNK_SIZE = 128; // Increased chunk size
    #pragma omp parallel for if(snowflakes.size() > 1000)
    for (size_t i = 0; i < snowflakes.size(); i += CHUNK_SIZE) {
        size_t end = std::min(i + CHUNK_SIZE, snowflakes.size());
        for (size_t j = i; j < end; ++j) {
            auto& snow = snowflakes[j];
            
            if (textureChanged && snow.settled && checkCollisions) {
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

            // Vectorized update of position
            snow.y += snow.speed * 0.7f;
            float drift_change = distrib_drift_rand(rng);
            snow.drift = std::clamp(snow.drift + drift_change, -1.0f, 1.0f);
            snow.x += snow.drift + (static_cast<float>(wind) * (snow.radius / 3.0f));
            snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);

            // Efficient boundary wrapping
            if (snow.x < -10) snow.x += screenWidth + 20;
            else if (snow.x > screenWidth + 10) snow.x -= screenWidth + 20;

            if (snow.y > screenHeight + 10) {
                snow = createSnowflake(screenWidth, screenHeight);
                snow.y = -10;
            }
        }
    }
    
    // Only sort and update vertex buffers when needed
    if (frameCount % SORT_INTERVAL == 0) {
        updateVertexBuffers();
    }
    
    frameCount = (frameCount + 1) % 1000; // Prevent potential overflow
}

void SnowSystem::updateVertexBuffers() {
    static std::vector<Snowflake> sortedSnowflakes;
    static bool firstRun = true;
    
    // Only perform full sort on first run or if significant changes occurred
    if (firstRun) {
        sortedSnowflakes = snowflakes;
        std::sort(sortedSnowflakes.begin(), sortedSnowflakes.end(),
                 [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });
        firstRun = false;
    }
    
    // Reserve space for batch groups if needed
    for (int i = 0; i < 3; ++i) {
        auto& group = batchGroups[i];
        size_t maxBatchVertices = static_cast<size_t>(MAX_BATCH_SIZE) * 4;
        size_t requiredSize = std::min(maxBatchVertices, snowflakes.size() * 4);
        if (group.vertices.capacity() < requiredSize) {
            group.vertices.reserve(requiredSize);
        }
        group.vertices.clear();
        group.count = 0;
    }
    
    // Update vertex buffers using SIMD-friendly struct-of-arrays approach
    alignas(16) float positions_x[4];
    alignas(16) float positions_y[4];
    
    for (const auto& snow : sortedSnowflakes) {
        int batchIndex = snow.radius - 1;
        auto& group = batchGroups[batchIndex];
        
        if (group.count >= MAX_BATCH_SIZE) continue;
        
        float size = snow.radius * 2.0f;
        float depthFactor = (snow.depth + 1.0f) * 0.5f;
        Uint8 alpha = static_cast<Uint8>((0.4f + 0.4f * depthFactor) * snow.alpha * 255);
        SDL_Color color = {255, 255, 255, alpha};
        
        // SIMD-friendly position calculation
        positions_x[0] = snow.x - size;
        positions_x[1] = snow.x + size;
        positions_x[2] = snow.x + size;
        positions_x[3] = snow.x - size;
        
        positions_y[0] = snow.y - size;
        positions_y[1] = snow.y - size;
        positions_y[2] = snow.y + size;
        positions_y[3] = snow.y + size;
        
        // Add vertices in a single batch
        group.vertices.insert(group.vertices.end(), {
            {{positions_x[0], positions_y[0]}, color, {0.0f, 0.0f}},
            {{positions_x[1], positions_y[1]}, color, {1.0f, 0.0f}},
            {{positions_x[2], positions_y[2]}, color, {1.0f, 1.0f}},
            {{positions_x[3], positions_y[3]}, color, {0.0f, 1.0f}}
        });
        
        group.count++;
    }
}

void SnowSystem::draw(SDL_Renderer* renderer) {
    static SDL_BlendMode currentBlendMode = SDL_BLENDMODE_NONE;
    
    // Set blend mode only once if needed
    if (currentBlendMode != SDL_BLENDMODE_BLEND) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        currentBlendMode = SDL_BLENDMODE_BLEND;
    }
    
    // Track if we rendered anything
    bool didRender = false;
    
    // Render batches from back to front
    for (int i = 0; i < 3; ++i) {
        const auto& group = batchGroups[i];
        if (group.count == 0) continue;
        
        // Skip nearly transparent batches
        bool hasVisibleSnowflakes = false;
        for (size_t j = 0; j < group.vertices.size(); j += 4) {
            if (group.vertices[j].color.a > 5) {
                hasVisibleSnowflakes = true;
                break;
            }
        }
        if (!hasVisibleSnowflakes) continue;
        
        // Render the batch
        SDL_RenderGeometry(renderer,
                          snowTextures[i],
                          group.vertices.data(),
                          group.vertices.size(),
                          group.indices.data(),
                          group.count * 6);
        
        didRender = true;
    }
    
    // Only reset blend mode if we actually rendered something
    if (didRender && currentBlendMode != SDL_BLENDMODE_NONE) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        currentBlendMode = SDL_BLENDMODE_NONE;
    }
}