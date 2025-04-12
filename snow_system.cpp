// snow_system.cpp
#include "snow_system.h"
#include "config.h" // Keep for screen dimensions if needed, or remove if passed in
#include <random>
#include <algorithm>
#include <iostream>
#include <vector> // Ensure vector is included

namespace {
    std::mt19937 rng{std::random_device{}()};

    // Helper to create snowflake textures (used during pre-rendering)
    SDL_Texture* createCircleTexture(SDL_Renderer* renderer, int radius, Uint8 alpha = 255) {
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
    }
} // end anonymous namespace

SnowSystem::SnowSystem(int flakes, int width, int height)
    : numFlakes(flakes), screenWidth(width), screenHeight(height), renderer(nullptr),
      currentFrameIndex(0), totalFrames(PRE_RENDER_SECONDS * PRE_RENDER_FPS) {
    preRenderedFrames.reserve(totalFrames);
}

SnowSystem::~SnowSystem() {
    for (SDL_Texture* frame : preRenderedFrames) {
        if (frame) {
            SDL_DestroyTexture(frame);
        }
    }
    preRenderedFrames.clear();
    // Keep createSnowflake logic for the pre-rendering phase
}

Snowflake SnowSystem::createSnowflake(int width, int height) {
    std::uniform_real_distribution<float> distrib_pos_x(-50.0f, static_cast<float>(width) + 50.0f); // Allow starting off-screen
    std::uniform_real_distribution<float> distrib_pos_y(-50.0f, static_cast<float>(height) + 50.0f);
    std::uniform_real_distribution<float> distrib_speed(0.5f * PRE_RENDER_FPS / 30.0f, 1.5f * PRE_RENDER_FPS / 30.0f); // Scale speed by FPS
    std::uniform_real_distribution<float> distrib_drift(-0.1f * PRE_RENDER_FPS / 30.0f, 0.1f * PRE_RENDER_FPS / 30.0f);
    std::uniform_real_distribution<float> distrib_alpha(0.3f, 0.8f);
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-0.5f * PRE_RENDER_FPS / 30.0f, 0.5f * PRE_RENDER_FPS / 30.0f);
    std::uniform_int_distribution<> distrib_radius(2, 4);
    std::uniform_real_distribution<float> distrib_depth(-1.0f, 1.0f); // Keep depth for sorting during pre-render

    return {
        distrib_pos_x(rng),
        distrib_pos_y(rng), // Start anywhere, including off-screen
        distrib_speed(rng), // Use scaled speed
        distrib_drift(rng), // Use scaled drift
        distrib_alpha(rng),
        distrib_angle(rng),
        distrib_angle_vel(rng), // Use scaled angular velocity
        distrib_radius(rng),
        // false, // settled removed
        // 0,     // settleTime removed
        distrib_depth(rng)
    };
}

void SnowSystem::initialize(SDL_Renderer* r) {
    renderer = r;
    if (!renderer) {
        std::cerr << "Invalid renderer provided to snow system" << std::endl;
        return;
    }

    std::cout << "Pre-rendering " << totalFrames << " frames of snow animation..." << std::endl;

    // --- Pre-rendering Phase ---

    // 1. Create snowflake textures (different sizes/alphas perhaps)
    SDL_Texture* snowTexSmall = createCircleTexture(renderer, 2, 200);
    SDL_Texture* snowTexMedium = createCircleTexture(renderer, 3, 220);
    SDL_Texture* snowTexLarge = createCircleTexture(renderer, 4, 240);
    if (!snowTexSmall || !snowTexMedium || !snowTexLarge) {
        std::cerr << "Failed to create base snowflake textures for pre-rendering." << std::endl;
        // Cleanup any created textures
        if (snowTexSmall) SDL_DestroyTexture(snowTexSmall);
        if (snowTexMedium) SDL_DestroyTexture(snowTexMedium);
        if (snowTexLarge) SDL_DestroyTexture(snowTexLarge);
        return;
    }
    SDL_Texture* snowTextures[] = { snowTexSmall, snowTexMedium, snowTexLarge }; // Index by radius - 2

    // 2. Create initial snowflakes
    std::vector<Snowflake> snowflakes;
    snowflakes.reserve(numFlakes);
    for (int i = 0; i < numFlakes; ++i) {
        snowflakes.push_back(createSnowflake(screenWidth, screenHeight));
    }
 
    // 3. Create snowflake textures (done above)
 
    // 4. Loop through frames, update physics, render to target, store frame
    SDL_BlendMode previousBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &previousBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // Ensure blending is enabled
 
    for (int frame = 0; frame < totalFrames; ++frame) {
        // Create the texture for this specific frame
        SDL_Texture* finalFrameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                          SDL_TEXTUREACCESS_TARGET,
                                                          screenWidth, screenHeight);
        if (!finalFrameTexture) {
             std::cerr << "Failed to create final frame texture " << frame << ": " << SDL_GetError() << std::endl;
             // TODO: Add proper cleanup on failure
             break;
        }
        SDL_SetTextureBlendMode(finalFrameTexture, SDL_BLENDMODE_BLEND); // Enable alpha blending
 
        // Set render target to this frame's texture
        SDL_SetRenderTarget(renderer, finalFrameTexture);
        // Clear target with transparency
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // Update snowflake physics (simplified, no settling, fixed wind = 0)
        std::uniform_real_distribution<float> distrib_drift_rand(-0.02f * PRE_RENDER_FPS / 30.0f, 0.02f * PRE_RENDER_FPS / 30.0f);
        double wind = 0.0; // No wind for pre-rendering

        std::vector<Snowflake> nextFrameSnowflakes;
        nextFrameSnowflakes.reserve(snowflakes.size());

        for (auto& snow : snowflakes) {
             snow.y += snow.speed; // Use pre-scaled speed

             float drift_change = distrib_drift_rand(rng);
             snow.drift = std::clamp(snow.drift + drift_change, -0.5f, 0.5f); // Clamp drift
             snow.x += snow.drift + (static_cast<float>(wind) * (snow.radius / 3.0f)); // Wind is 0

             // Wrap around screen edges (allow going further off-screen before reset)
             if (snow.x < -snow.radius * 2 - 50) snow.x = screenWidth + snow.radius * 2 + 50;
             else if (snow.x > screenWidth + snow.radius * 2 + 50) snow.x = -snow.radius * 2 - 50;

             // Reset snowflake if it goes too far below screen
             if (snow.y > screenHeight + snow.radius * 2 + 50) {
                 nextFrameSnowflakes.push_back(createSnowflake(screenWidth, screenHeight));
                 nextFrameSnowflakes.back().y = -snow.radius * 2.0f - 50.0f; // Start above screen
             } else {
                 snow.angle = std::fmod(snow.angle + snow.angleVel, 360.0f);
                 nextFrameSnowflakes.push_back(snow);
             }
        }
        snowflakes.swap(nextFrameSnowflakes); // Update snowflake list for next iteration

        // Sort by depth for correct drawing order
        std::sort(snowflakes.begin(), snowflakes.end(),
                  [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });

        // Draw snowflakes to the target texture
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
            // Apply alpha based on snowflake property - Restore alpha modulation
            SDL_SetTextureColorMod(currentSnowTex, 255, 255, 255); // Ensure snow texture is white
            SDL_SetTextureAlphaMod(currentSnowTex, static_cast<Uint8>(snow.alpha * 255));
            SDL_RenderCopyEx(renderer, currentSnowTex, nullptr, &destRect, snow.angle, nullptr, SDL_FLIP_NONE);
        }
        // Reset render target (finished drawing snowflakes onto finalFrameTexture)
        SDL_SetRenderTarget(renderer, nullptr);
 
        // Store the completed frame texture
        preRenderedFrames.push_back(finalFrameTexture);
 
        // Optional: Print progress
        if ((frame + 1) % PRE_RENDER_FPS == 0) {
             std::cout << "Pre-rendered " << (frame + 1) / PRE_RENDER_FPS << " seconds..." << std::endl;
        }
    }

    // --- Create Seamless Loop ---
    // The 'snowflakes' vector now holds the state *after* the last frame's simulation.
    // Render one more frame using this state and replace frame 0 with it.
    // We need the original snowTextures here, so cleanup is moved after this block.
    std::cout << "Creating seamless loop frame..." << std::endl;
    SDL_Texture* loopFrameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                      SDL_TEXTUREACCESS_TARGET,
                                                      screenWidth, screenHeight);
    if (!loopFrameTexture) {
        std::cerr << "Failed to create loop frame texture: " << SDL_GetError() << std::endl;
        // Handle error: maybe skip looping or cleanup partially rendered frames?
    } else {
        SDL_SetTextureBlendMode(loopFrameTexture, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(renderer, loopFrameTexture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Clear with transparency
        SDL_RenderClear(renderer);

        // Draw the final snowflake state to the loop frame texture
        // Sort by depth again just in case (though state hasn't changed since last sort)
        std::sort(snowflakes.begin(), snowflakes.end(),
                  [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });

        // Use the original snowTextures array (cleanup moved below)
        // The snowTextures array (containing snowTexSmall, snowTexMedium, snowTexLarge)
        // is still in scope here because we moved its cleanup.
        for (const auto& snow : snowflakes) {
            int texIndex = snow.radius - 2;
            if (texIndex < 0 || texIndex > 2) continue;

            // Use the original textures created before the main loop
            SDL_Texture* currentSnowTex = snowTextures[texIndex];
                int texW, texH;
                SDL_QueryTexture(currentSnowTex, NULL, NULL, &texW, &texH);

                SDL_Rect destRect = {
                    static_cast<int>(snow.x - texW / 2.0f),
                    static_cast<int>(snow.y - texH / 2.0f),
                    texW,
                    texH
                };
                SDL_SetTextureColorMod(currentSnowTex, 255, 255, 255);
                SDL_SetTextureAlphaMod(currentSnowTex, static_cast<Uint8>(snow.alpha * 255));
                SDL_RenderCopyEx(renderer, currentSnowTex, nullptr, &destRect, snow.angle, nullptr, SDL_FLIP_NONE);
            }

            // No need to clean up temporary loop textures as we used the originals

            SDL_SetRenderTarget(renderer, nullptr); // Reset render target

            // Replace the original frame 0
            if (!preRenderedFrames.empty() && preRenderedFrames[0]) {
                SDL_DestroyTexture(preRenderedFrames[0]);
                preRenderedFrames[0] = loopFrameTexture;
                std::cout << "Frame 0 replaced for seamless loop." << std::endl;
            } else {
                 std::cerr << "Error: Could not replace frame 0 for looping." << std::endl;
                 SDL_DestroyTexture(loopFrameTexture); // Clean up unused loop frame
            }
        }
    }
    // --- End Seamless Loop Creation ---

    // 5. Cleanup temporary resources (base snowflake textures) - Now done AFTER loop creation
    SDL_DestroyTexture(snowTexSmall);
    SDL_DestroyTexture(snowTexMedium);
    SDL_DestroyTexture(snowTexLarge);
    snowflakes.clear(); // No longer needed

    SDL_SetRenderDrawBlendMode(renderer, previousBlendMode); // Restore blend mode

    std::cout << "Snow pre-rendering complete." << std::endl;
}


// Update simply advances the frame index
void SnowSystem::update() {
    if (totalFrames > 0) {
        currentFrameIndex = (currentFrameIndex + 1) % totalFrames;
    }
}

// Draw renders the current pre-rendered frame
void SnowSystem::draw(SDL_Renderer* renderer) {
    if (!renderer || preRenderedFrames.empty() || currentFrameIndex >= preRenderedFrames.size()) {
        return;
    }

    // --- DEBUG code removed ---

    // Original code restored:
    SDL_Texture* currentFrame = preRenderedFrames[currentFrameIndex];
    if (currentFrame) {
        // Ensure blend mode is set correctly for rendering the transparent frame
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        // Explicitly reset color and alpha modulation for the pre-rendered frame texture
        SDL_SetTextureColorMod(currentFrame, 255, 255, 255);
        SDL_SetTextureAlphaMod(currentFrame, 255);
        SDL_RenderCopy(renderer, currentFrame, nullptr, nullptr);
    }
}
