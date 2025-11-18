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
#include <algorithm>

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
    , httpClient(std::make_unique<HTTPClient>(BACKGROUND_API_URL_HOST, BACKGROUND_API_URL_PORT, true))
{
    LOG_INFO("BackgroundManager initialized with shared HTTPClient instance");
}

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
    
    // Join worker thread if it exists
    if (workerThread.joinable()) {
        LOG_DEBUG("Waiting for worker thread to finish...");
        // We use a short timeout join simulation here since std::thread doesn't support timed_join
        // In a real destructor we should join, but if it's stuck we might hang.
        // Given the design, we'll just join and hope the cancellation flag works.
        workerThread.join();
        LOG_DEBUG("Worker thread joined successfully");
    }
    
    // Clean up HTTP client
    if (httpClient) {
        LOG_DEBUG("Cleaning up shared HTTPClient instance");
        httpClient.reset();
    }
    
    // Clean up SDL resources (now safe - no thread accessing them)
    LOG_DEBUG("Cleaning up SDL resources");
    if (currentTexture) {
        SDL_DestroyTexture(currentTexture);
        currentTexture = nullptr;
    }
    if (overlayTexture) {
        SDL_DestroyTexture(overlayTexture);
        overlayTexture = nullptr;
    }
    if (currentImage) {
        SDL_FreeSurface(currentImage);
        currentImage = nullptr;
    }
    if (overlay) {
        SDL_FreeSurface(overlay);
        overlay = nullptr;
    }
    if (pendingImage) {
        SDL_FreeSurface(pendingImage);
        pendingImage = nullptr;
    }
    
    LOG_INFO("BackgroundManager destroyed");
}

std::string BackgroundManager::fetchImageUrl() {
    // Use shared httpClient with mutex protection for thread safety
    std::lock_guard<std::mutex> clientLock(httpClientMutex);
    
    auto response = httpClient->get(BACKGROUND_API_URL_PATH);
    
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
    
    // Set aggressive timeouts to prevent hangs
    cli.set_read_timeout(10, 0);
    cli.set_write_timeout(10, 0);
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

void BackgroundManager::startBackgroundUpdate(int width, int height) {
    // Skip if already loading
    if (isLoading.load()) {
        LOG_DEBUG("Background update already in progress, skipping");
        return;
    }
    
    // Clean up previous thread if it exists
    if (workerThread.joinable()) {
        LOG_DEBUG("Joining previous worker thread");
        workerThread.join();
    }
    
    LOG_INFO("Starting background update");
    isLoading.store(true);
    shouldStopThread.store(false);
    lastThreadStart.store(time(nullptr));
    
    workerThread = std::thread([this, width, height]() {
        LOG_DEBUG("Worker thread started");
        
        try {
            if (shouldStopThread.load()) {
                isLoading.store(false);
                return;
            }
            
            // Step 1: Fetch URL
            std::string imageUrl = fetchImageUrl();
            
            if (shouldStopThread.load()) {
                isLoading.store(false);
                return;
            }
            
            if (!imageUrl.empty()) {
                // Step 2: Load Image
                SDL_Surface* newImage = loadImage(imageUrl, width, height);
                
                if (shouldStopThread.load()) {
                    if (newImage) SDL_FreeSurface(newImage);
                    isLoading.store(false);
                    return;
                }
                
                if (newImage) {
                    LOG_INFO("Successfully loaded background image (%dx%d)", newImage->w, newImage->h);
                    
                    std::lock_guard<std::mutex> lock(mutex);
                    if (pendingImage) {
                        SDL_FreeSurface(pendingImage);
                    }
                    pendingImage = newImage;
                    pendingImageReady = true;
                    consecutiveFailures.store(0);
                } else {
                    LOG_ERROR("Failed to load background image from: %s", imageUrl.c_str());
                    // Count as failure
                    std::lock_guard<std::mutex> lock(mutex);
                    consecutiveFailures.fetch_add(1);
                    lastFailedAttempt.store(time(nullptr));
                }
            } else {
                LOG_ERROR("Failed to fetch image URL");
                std::lock_guard<std::mutex> lock(mutex);
                consecutiveFailures.fetch_add(1);
                lastFailedAttempt.store(time(nullptr));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in worker thread: %s", e.what());
        } catch (...) {
            LOG_ERROR("Unknown exception in worker thread");
        }
        
        isLoading.store(false);
        LOG_DEBUG("Worker thread finished");
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
    } // Mutex released
    
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
        LOG_WARNING("Worker thread running for %ld seconds (timeout is %d). It should finish soon due to HTTP timeouts.", 
                  currentTime - lastStart, THREAD_TIMEOUT);
        
        // Do NOT detach. We must wait for it to finish to avoid use-after-free.
        // The HTTP client timeouts (10s) should ensure it doesn't hang forever.
        // We just skip starting a new update until this one finishes.
        return;
    }
    
    // Check if we need to start a new update
    if (!isLoading.load() && 
        (difftime(currentTime, lastUpdate) > BACKGROUND_UPDATE_INTERVAL || currentImage == nullptr)) {
        
        // Backoff logic
        const int failures = consecutiveFailures.load();
        const time_t lastFailureTime = lastFailedAttempt.load();
        int backoffDelay = std::min(30 * failures, 600);
        
        if (failures > 0 && difftime(currentTime, lastFailureTime) < backoffDelay) {
            return;
        }
        
        lastUpdate = currentTime;
        startBackgroundUpdate(width, height);
    }
}

void BackgroundManager::draw(SDL_Renderer* renderer) {
    if (!renderer) {
        LOG_ERROR("draw() called with null renderer");
        return;
    }

    // Update textures if needed (only when image changes)
    updateTextures(renderer);
    
    // Always render something - either the image or fallback color
    if (currentTexture) {
        // Render the background image
        if (SDL_RenderCopy(renderer, currentTexture, NULL, NULL) != 0) {
            LOG_ERROR("Failed to render background texture: %s", SDL_GetError());
        }
        
        // Render the darkening overlay
        if (overlayTexture) {
            if (SDL_RenderCopy(renderer, overlayTexture, NULL, NULL) != 0) {
                LOG_ERROR("Failed to render overlay texture: %s", SDL_GetError());
            }
        }
    } else {
        // Fallback: render solid color background
        SDL_SetRenderDrawColor(renderer, FALLBACK_BG_RED, FALLBACK_BG_GREEN, FALLBACK_BG_BLUE, 255);
        SDL_RenderClear(renderer);
        
        static bool fallbackLogged = false;
        if (!fallbackLogged) {
            LOG_DEBUG("Using fallback background color (no image available yet)");
            fallbackLogged = true;
        }
    }
}