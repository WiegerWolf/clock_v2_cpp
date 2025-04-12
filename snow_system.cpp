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
    // std::uniform_real_distribution<float> distrib_alpha(0.3f, 0.8f); // Removed
    std::uniform_real_distribution<float> distrib_angle(0.0f, 360.0f);
    std::uniform_real_distribution<float> distrib_angle_vel(-0.5f * PRE_RENDER_FPS / 30.0f, 0.5f * PRE_RENDER_FPS / 30.0f);
    std::uniform_int_distribution<> distrib_radius(2, 4);
    std::uniform_real_distribution<float> distrib_depth(-1.0f, 1.0f); // Keep depth for sorting during pre-render

    return {
        distrib_pos_x(rng),
        distrib_pos_y(rng), // Start anywhere, including off-screen
        distrib_speed(rng), // Use scaled speed
        distrib_drift(rng), // Use scaled drift
        // distrib_alpha(rng), // Removed
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

    // Temporary storage for frames 1 to N-1
    std::vector<SDL_Texture*> tempFrames;
    tempFrames.reserve(totalFrames > 0 ? totalFrames - 1 : 0); // Reserve space if totalFrames > 0

    // --- Loop for frames 1 to N-1 ---
    for (int frame = 1; frame < totalFrames; ++frame) {
        // Create the texture for this specific frame (frame 'frame')
        SDL_Texture* frameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                      SDL_TEXTUREACCESS_TARGET,
                                                      screenWidth, screenHeight);
        if (!frameTexture) {
             std::cerr << "Failed to create frame texture " << frame << ": " << SDL_GetError() << std::endl;
             // Cleanup already created frames in tempFrames before breaking
             for (SDL_Texture* createdFrame : tempFrames) {
                 if (createdFrame) SDL_DestroyTexture(createdFrame);
             }
             tempFrames.clear(); // Clear the vector
             // Also clean up base textures as initialization failed
             SDL_DestroyTexture(snowTexSmall);
             SDL_DestroyTexture(snowTexMedium);
             SDL_DestroyTexture(snowTexLarge);
             SDL_SetRenderTarget(renderer, nullptr); // Reset render target
             SDL_SetRenderDrawBlendMode(renderer, previousBlendMode); // Restore blend mode
             return; // Exit initialize function entirely on failure
        }
        SDL_SetTextureBlendMode(frameTexture, SDL_BLENDMODE_BLEND); // Enable alpha blending

        // Set render target to this frame's texture
        SDL_SetRenderTarget(renderer, frameTexture);
        // Clear target with transparency
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Use transparent black
        SDL_RenderClear(renderer);

        // --- Update snowflake physics (advances state from frame-1 to frame) ---
        std::uniform_real_distribution<float> distrib_drift_rand(-0.02f * PRE_RENDER_FPS / 30.0f, 0.02f * PRE_RENDER_FPS / 30.0f);
        double wind = 0.0; // No wind for pre-rendering

        std::vector<Snowflake> nextFrameSnowflakes; // Store updated snowflakes here
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
            // Apply alpha based on snowflake property - Now always opaque
            SDL_SetTextureColorMod(currentSnowTex, 255, 255, 255); // Ensure snow texture is white
            SDL_SetTextureAlphaMod(currentSnowTex, 255); // Set alpha to fully opaque
            SDL_RenderCopyEx(renderer, currentSnowTex, nullptr, &destRect, snow.angle, nullptr, SDL_FLIP_NONE);
        }

        // Reset render target (finished drawing snowflakes onto frameTexture)
        SDL_SetRenderTarget(renderer, nullptr);

        // Store the completed frame texture (frame 'frame')
        tempFrames.push_back(frameTexture);

        // Optional: Print progress (adjust frame index for message)
        if (frame % PRE_RENDER_FPS == 0 && frame > 0) {
             std::cout << "Pre-rendered " << frame / PRE_RENDER_FPS << " seconds..." << std::endl;
        }
    }

    // --- Render Frame 0 using the final state from the loop ---
    // 'snowflakes' now holds the state needed for frame 0

    SDL_Texture* frame0Texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                  SDL_TEXTUREACCESS_TARGET,
                                                  screenWidth, screenHeight);
    if (!frame0Texture) {
         std::cerr << "Failed to create frame 0 texture: " << SDL_GetError() << std::endl;
         // Cleanup tempFrames and base textures
         for (SDL_Texture* tempFrame : tempFrames) { if (tempFrame) SDL_DestroyTexture(tempFrame); }
         tempFrames.clear();
         SDL_DestroyTexture(snowTexSmall);
         SDL_DestroyTexture(snowTexMedium);
         SDL_DestroyTexture(snowTexLarge);
         SDL_SetRenderDrawBlendMode(renderer, previousBlendMode);
         return;
    }
    SDL_SetTextureBlendMode(frame0Texture, SDL_BLENDMODE_BLEND);

    // Set render target to frame 0 texture
    SDL_SetRenderTarget(renderer, frame0Texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    // Sort the final state for frame 0 drawing
    std::sort(snowflakes.begin(), snowflakes.end(),
              [](const Snowflake& a, const Snowflake& b) { return a.depth < b.depth; });

    // Draw snowflakes for frame 0 (using the state after frame N-1 update)
    for (const auto& snow : snowflakes) {
        int texIndex = snow.radius - 2;
        if (texIndex < 0 || texIndex > 2) continue;

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
        SDL_SetTextureAlphaMod(currentSnowTex, 255);
        SDL_RenderCopyEx(renderer, currentSnowTex, nullptr, &destRect, snow.angle, nullptr, SDL_FLIP_NONE);
    }

    // Reset render target
    SDL_SetRenderTarget(renderer, nullptr);

    // --- Assemble final preRenderedFrames vector ---
    preRenderedFrames.clear(); // Ensure it's empty before assembly
    preRenderedFrames.reserve(totalFrames);
    if (frame0Texture) { // Add frame 0 first if it was created successfully
        preRenderedFrames.push_back(frame0Texture);
    }
    // Add frames 1 to N-1 from temp storage
    preRenderedFrames.insert(preRenderedFrames.end(), tempFrames.begin(), tempFrames.end());
    tempFrames.clear(); // Clear temp vector as textures are now owned by preRenderedFrames

    // Seamless loop is now handled by fading in the draw function.
    // No need to replace frame 0 here.

    // 5. Cleanup temporary resources (base snowflake textures)
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

    SDL_Texture* currentFrameTexture = preRenderedFrames[currentFrameIndex];
    if (!currentFrameTexture) return;

    // Ensure blend mode is set correctly for rendering the transparent frame(s)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Calculate the start index for fading
    int fadeStartIndex = totalFrames - FADE_FRAMES;

    if (FADE_FRAMES > 0 && currentFrameIndex >= fadeStartIndex) {
        // --- Fading Period ---
        // Calculate blend factor (0.0 at fadeStartIndex, approaches 1.0 at totalFrames-1)
        float alphaBlend = static_cast<float>(currentFrameIndex - fadeStartIndex) / FADE_FRAMES;
        alphaBlend = std::clamp(alphaBlend, 0.0f, 1.0f); // Ensure it stays within [0, 1]

        // Get the corresponding frame from the beginning of the loop
        int loopFrameIndex = currentFrameIndex - fadeStartIndex;
        // Ensure loopFrameIndex is valid
        if (loopFrameIndex < 0 || loopFrameIndex >= preRenderedFrames.size()) {
             loopFrameIndex = 0; // Fallback, though shouldn't be needed
        }
        SDL_Texture* loopFrameTexture = preRenderedFrames[loopFrameIndex];

        if (loopFrameTexture) {
            // 1. Draw the current frame (fading out)
            SDL_SetTextureColorMod(currentFrameTexture, 255, 255, 255);
            SDL_SetTextureAlphaMod(currentFrameTexture, static_cast<Uint8>((1.0f - alphaBlend) * 255.0f));
            SDL_RenderCopy(renderer, currentFrameTexture, nullptr, nullptr);

            // 2. Draw the corresponding loop frame (fading in) on top
            SDL_SetTextureColorMod(loopFrameTexture, 255, 255, 255);
            SDL_SetTextureAlphaMod(loopFrameTexture, static_cast<Uint8>(alphaBlend * 255.0f));
            SDL_RenderCopy(renderer, loopFrameTexture, nullptr, nullptr);
        } else {
            // Fallback if loop frame is missing: just draw current frame fully opaque
            SDL_SetTextureColorMod(currentFrameTexture, 255, 255, 255);
            SDL_SetTextureAlphaMod(currentFrameTexture, 255);
            SDL_RenderCopy(renderer, currentFrameTexture, nullptr, nullptr);
        }

    } else {
        // --- Normal Period (No Fading) ---
        // Explicitly reset color and alpha modulation for the pre-rendered frame texture
        SDL_SetTextureColorMod(currentFrameTexture, 255, 255, 255);
        SDL_SetTextureAlphaMod(currentFrameTexture, 255);
        SDL_RenderCopy(renderer, currentFrameTexture, nullptr, nullptr);
    }
}
