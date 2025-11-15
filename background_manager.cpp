#include "background_manager.h"
#include "config.h"
#include "constants.h"
#include "logger.h"
#include "http_client.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <httplib.h>
#include <SDL2/SDL_image.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;

BackgroundManager::BackgroundManager() 
    : currentImage(nullptr)
    , overlay(nullptr)
    , pendingImage(nullptr)
    , currentTexture(nullptr)
    , overlayTexture(nullptr)
    , cachedRenderer(nullptr)
    , lastUpdate(0)
    , error("")
{}

std::string BackgroundManager::getError() const {
    std::lock_guard<std::mutex> lock(mutex);
    return error;
}

BackgroundManager::~BackgroundManager() {
    LOG_INFO("BackgroundManager destructor called");
    
    // Signal thread to stop
    shouldStopThread.store(true);
    
    // Wait for any texture updates to complete
    int waitCount = 0;
    while (textureUpdateInProgress.load() && waitCount++ < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Give thread 2 seconds to finish gracefully
    if (backgroundThread.joinable()) {
        LOG_DEBUG("Waiting for background thread to finish...");
        
        // Try to join with timeout
        std::thread waiter([this]() {
            if (backgroundThread.joinable()) {
                backgroundThread.join();
            }
        });
        waiter.detach();
        
        // Wait maximum 2 seconds
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Force detach if still running
        if (backgroundThread.joinable()) {
            LOG_WARNING("Background thread did not finish in time, detaching forcefully");
            backgroundThread.detach();
        }
    }
    
    // Clean up SDL resources (now safe - no thread accessing them)
    LOG_DEBUG("Cleaning up SDL resources");
    if (currentTexture) SDL_DestroyTexture(currentTexture);
    if (overlayTexture) SDL_DestroyTexture(overlayTexture);
    if (currentImage) SDL_FreeSurface(currentImage);
    if (overlay) SDL_FreeSurface(overlay);
    if (pendingImage) SDL_FreeSurface(pendingImage);
    
    LOG_INFO("BackgroundManager destroyed");
}

std::string BackgroundManager::fetchImageUrl() {
    HTTPClient client(BACKGROUND_API_URL_HOST, BACKGROUND_API_URL_PORT, true);
    
    auto response = client.get(BACKGROUND_API_URL_PATH);
    
    if (!response.success) {
        std::lock_guard<std::mutex> lock(mutex);
        error = "Failed to fetch image URL: " + response.error;
        LOG_ERROR("%s", error.c_str());
        return "";
    }
    
    if (response.statusCode != 200) {
        std::lock_guard<std::mutex> lock(mutex);
        error = "HTTP status: " + std::to_string(response.statusCode);
        LOG_ERROR("Background API returned status %d", response.statusCode);
        return "";
    }
    
    try {
        json data = json::parse(response.body);
        std::string url = data[0]["fullUrl"].get<std::string>();
        LOG_DEBUG("Fetched background image URL: %s", url.c_str());
        return url;
    } catch (const json::parse_error& e) {
        std::lock_guard<std::mutex> lock(mutex);
        error = "JSON parse error: " + std::string(e.what());
        LOG_ERROR("%s", error.c_str());
        return "";
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(mutex);
        error = "Error processing JSON: " + std::string(e.what());
        LOG_ERROR("%s", error.c_str());
        return "";
    }
}

SDL_Surface* BackgroundManager::createDarkeningOverlay(int width, int height) {
    SDL_Surface* overlaySurface = SDL_CreateRGBSurface(0, width, height, 32, 
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!overlaySurface) {
        std::lock_guard<std::mutex> lock(mutex);
        error = "SDL_CreateRGBSurface failed: " + std::string(SDL_GetError());
        return nullptr;
    }
    SDL_FillRect(overlaySurface, NULL, SDL_MapRGBA(overlaySurface->format, 0, 0, 0, 
        static_cast<int>(255 * BACKGROUND_DARKNESS)));
    return overlaySurface;
}

SDL_Surface* BackgroundManager::loadImage(const std::string& url, int width, int height) {
    LOG_DEBUG("loadImage() called for URL: %s", url.c_str());
    
    std::string hostWithProto = url.substr(0, url.find("/", 8));
    std::string host = hostWithProto.substr(hostWithProto.find("://") + 3);
    std::string path = url.substr(url.find("/", 8));
    
    httplib::SSLClient cli(host);
    cli.set_follow_location(true);
    cli.enable_server_certificate_verification(false);
    
    // Set aggressive timeouts to prevent hangs
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);
    cli.set_connection_timeout(5, 0);
    
    LOG_DEBUG("Fetching image from host: %s, path: %s", host.c_str(), path.c_str());
    
    auto res = cli.Get(path.c_str());
    if (!res) {
        std::lock_guard<std::mutex> lock(mutex);
        error = "Failed to get image response: " + std::string(httplib::to_string(res.error()));
        LOG_ERROR("HTTP GET failed: %s", httplib::to_string(res.error()).c_str());
        return nullptr;
    }
    
    if (res && res->status == 200) {
        SDL_RWops* rw = SDL_RWFromMem((void*)res->body.data(), res->body.size());
        if (!rw) {
            std::lock_guard<std::mutex> lock(mutex);
            error = "SDL_RWFromMem failed: " + std::string(SDL_GetError());
            return nullptr;
        }
        
        SDL_Surface* imageSurface = IMG_Load_RW(rw, 1);
        if (!imageSurface) {
            std::lock_guard<std::mutex> lock(mutex);
            error = "IMG_Load_RW failed: " + std::string(IMG_GetError());
            return nullptr;
        }
        
        SDL_Surface* scaledSurface = SDL_CreateRGBSurface(0, width, height, 32, 
            imageSurface->format->Rmask, imageSurface->format->Gmask, 
            imageSurface->format->Bmask, imageSurface->format->Amask);
        if (!scaledSurface) {
            std::lock_guard<std::mutex> lock(mutex);
            error = "SDL_CreateRGBSurface (scaled) failed: " + std::string(SDL_GetError());
            SDL_FreeSurface(imageSurface);
            return nullptr;
        }
        
        if (SDL_BlitScaled(imageSurface, NULL, scaledSurface, NULL) < 0) {
            std::lock_guard<std::mutex> lock(mutex);
            error = "SDL_BlitScaled failed: " + std::string(SDL_GetError());
            SDL_FreeSurface(imageSurface);
            SDL_FreeSurface(scaledSurface);
            return nullptr;
        }
        
        SDL_FreeSurface(imageSurface);
        
        // Create overlay (without locking - it's thread-local)
        SDL_Surface* newOverlay = createDarkeningOverlay(width, height);
        if (newOverlay) {
            std::lock_guard<std::mutex> lock(mutex);
            if (overlay) SDL_FreeSurface(overlay);
            overlay = newOverlay;
        }
        
        return scaledSurface;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    error = "HTTP image request failed: " + std::to_string(res ? res->status : -1);
    return nullptr;
}

void BackgroundManager::loadImageAsync(const std::string& url, int width, int height) {
    // Skip if already loading
    if (isLoading.load()) {
        LOG_DEBUG("Background image load already in progress, skipping");
        return;
    }
    
    // Check thread count limit
    if (threadCount.load() >= MAX_THREADS) {
        LOG_WARNING("Max background threads reached (%d), skipping image load", MAX_THREADS);
        return;
    }
    
    // Check if previous thread timed out
    time_t now = time(nullptr);
    time_t lastStart = lastThreadStart.load();
    if (lastStart > 0 && (now - lastStart) < THREAD_TIMEOUT) {
        // Previous thread still might be running
        if (backgroundThread.joinable()) {
            LOG_WARNING("Previous background thread still running after %ld seconds, detaching", now - lastStart);
            backgroundThread.detach(); // Don't block, just detach
        }
    }
    
    // Join previous thread if it finished
    if (backgroundThread.joinable()) {
        // Try to join with zero timeout (non-blocking check)
        backgroundThread.detach(); // Always detach to avoid blocking
    }
    
    LOG_INFO("Starting background image load from: %s", url.c_str());
    lastThreadStart.store(now);
    threadCount.fetch_add(1);
    isLoading.store(true);
    
    backgroundThread = std::thread([this, url, width, height]() {
        LOG_DEBUG("Background thread started, ThreadID: %ld", std::this_thread::get_id());
        
        SDL_Surface* newImage = loadImage(url, width, height);
        
        if (newImage) {
            LOG_INFO("Successfully loaded background image (%dx%d)", newImage->w, newImage->h);
            
            // Critical section: set pending image with ready flag
            {
                std::lock_guard<std::mutex> lock(mutex);
                
                // Free any existing pending image that wasn't processed
                if (pendingImage) {
                    LOG_WARNING("Previous pendingImage was not processed, freeing it");
                    SDL_FreeSurface(pendingImage);
                }
                
                pendingImage = newImage;
                pendingImageReady = true;  // Signal main thread to process it
            }
        } else {
            LOG_ERROR("Failed to load background image from: %s", url.c_str());
        }
        
        isLoading.store(false);
        threadCount.fetch_sub(1);
        LOG_DEBUG("Background thread finished, ThreadID: %ld", std::this_thread::get_id());
    });
}

void BackgroundManager::updateTextures(SDL_Renderer* renderer) {
    // Prevent concurrent texture updates
    if (textureUpdateInProgress.exchange(true)) {
        return; // Another update already in progress
    }
    
    SDL_Surface* surfaceToProcess = nullptr;
    SDL_Surface* oldSurface = nullptr;
    
    // Critical section: transfer ownership of pending surface
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (pendingImageReady && pendingImage) {
            LOG_DEBUG("Processing pending background image");
            
            // Transfer ownership
            surfaceToProcess = pendingImage;
            oldSurface = currentImage;
            
            // Clear pending state
            pendingImage = nullptr;
            pendingImageReady = false;
            currentImage = surfaceToProcess;  // Update current immediately
        }
    } // Mutex released - background thread can now set new pendingImage
    
    // Now we own surfaceToProcess exclusively - safe to use for SDL operations
    if (surfaceToProcess) {
        // Validate surface before SDL operations
        if (surfaceToProcess->w > 0 && surfaceToProcess->h > 0) {
            LOG_INFO("Creating texture from surface (%dx%d)", 
                     surfaceToProcess->w, surfaceToProcess->h);
            
            // Destroy old texture
            if (currentTexture) {
                SDL_DestroyTexture(currentTexture);
                currentTexture = nullptr;
            }
            
            // Create new texture (MAIN THREAD ONLY - SAFE)
            SDL_Texture* newTexture = SDL_CreateTextureFromSurface(renderer, surfaceToProcess);
            if (newTexture) {
                currentTexture = newTexture;
                LOG_DEBUG("Texture created successfully");
                
                // Update overlay texture as well
                if (overlayTexture) {
                    SDL_DestroyTexture(overlayTexture);
                }
                if (overlay) {
                    overlayTexture = SDL_CreateTextureFromSurface(renderer, overlay);
                    if (!overlayTexture) {
                        LOG_ERROR("Failed to create overlay texture: %s", SDL_GetError());
                    }
                }
            } else {
                LOG_ERROR("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
            }
        } else {
            LOG_ERROR("Invalid surface dimensions: %dx%d", 
                     surfaceToProcess->w, surfaceToProcess->h);
        }
        
        // Free old surface if it exists
        if (oldSurface) {
            SDL_FreeSurface(oldSurface);
            LOG_DEBUG("Freed old background surface");
        }
    }
    
    // Handle renderer changes (recreate textures if needed)
    if (cachedRenderer != renderer) {
        LOG_INFO("Renderer changed, recreating textures");
        cachedRenderer = renderer;
        
        // Destroy old textures
        if (currentTexture) {
            SDL_DestroyTexture(currentTexture);
            currentTexture = nullptr;
        }
        if (overlayTexture) {
            SDL_DestroyTexture(overlayTexture);
            overlayTexture = nullptr;
        }
        
        // Recreate from surfaces if available
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (currentImage && currentImage->w > 0 && currentImage->h > 0) {
                currentTexture = SDL_CreateTextureFromSurface(renderer, currentImage);
                if (!currentTexture) {
                    LOG_ERROR("Failed to recreate current texture: %s", SDL_GetError());
                }
            }
            if (overlay && overlay->w > 0 && overlay->h > 0) {
                overlayTexture = SDL_CreateTextureFromSurface(renderer, overlay);
                if (!overlayTexture) {
                    LOG_ERROR("Failed to recreate overlay texture: %s", SDL_GetError());
                }
            }
        }
    }
    
    // Reset the texture update flag
    textureUpdateInProgress.store(false);
}

void BackgroundManager::update(int width, int height) {
    time_t currentTime;
    time(&currentTime);
    
    // Check for hung thread
    time_t lastStart = lastThreadStart.load();
    if (isLoading.load() && lastStart > 0 && (currentTime - lastStart) > THREAD_TIMEOUT) {
        LOG_ERROR("Background thread appears hung (running for %ld seconds), forcing reset", 
                  currentTime - lastStart);
        isLoading.store(false);
        if (backgroundThread.joinable()) {
            backgroundThread.detach();
        }
        threadCount.store(0);
    }
    
    // Check if we need to start loading a new image
    if (!isLoading && (difftime(currentTime, lastUpdate) > BACKGROUND_UPDATE_INTERVAL || currentImage == nullptr)) {
        std::string imageUrl = fetchImageUrl();
        if (!imageUrl.empty()) {
            loadImageAsync(imageUrl, width, height);
            lastUpdate = currentTime;
        }
    }
}

void BackgroundManager::draw(SDL_Renderer* renderer) {
    // Update textures if needed (only when image changes)
    updateTextures(renderer);
    if (!renderer) {
        LOG_ERROR("draw() called with null renderer");
        return;
    }
    
    // Render with null checks
    if (currentTexture) {
        if (SDL_RenderCopy(renderer, currentTexture, NULL, NULL) != 0) {
            LOG_ERROR("Failed to render background texture: %s", SDL_GetError());
        }
    }
    
    if (overlayTexture) {
        if (SDL_RenderCopy(renderer, overlayTexture, NULL, NULL) != 0) {
            LOG_ERROR("Failed to render overlay texture: %s", SDL_GetError());
        }
    }
}