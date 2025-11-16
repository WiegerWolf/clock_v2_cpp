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
    , lastFailedAttempt(0)
    , consecutiveFailures(0)
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
    
    // Signal thread to stop (if checked inside worker)
    shouldStopThread.store(true);
    
    // Wait for any texture updates to complete
    int waitCount = 0;
    while (textureUpdateInProgress.load() && waitCount++ < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Join fetch thread if it exists
    if (fetchThread.joinable()) {
        LOG_DEBUG("Waiting for fetch thread to finish...");
        fetchThread.join();
        LOG_DEBUG("Fetch thread joined successfully");
    }
    
    // Join worker thread if it exists - simple and safe
    if (backgroundThread.joinable()) {
        LOG_DEBUG("Waiting for background thread to finish...");
        backgroundThread.join();
        LOG_DEBUG("Background thread joined successfully");
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

void BackgroundManager::fetchImageUrlAsync(int width, int height) {
    // Skip if already fetching
    if (isFetching.load()) {
        LOG_DEBUG("URL fetch already in progress, skipping");
        return;
    }
    
    // Join previous fetch thread if it exists
    if (fetchThread.joinable()) {
        LOG_DEBUG("Joining previous fetch thread before starting new one");
        fetchThread.join();
        LOG_DEBUG("Previous fetch thread joined successfully");
    }
    
    LOG_INFO("Starting asynchronous URL fetch");
    isFetching.store(true);
    
    fetchThread = std::thread([this, width, height]() {
        LOG_DEBUG("Fetch thread started");
        
        // Check if we should stop
        if (shouldStopThread.load()) {
            LOG_DEBUG("Fetch thread cancelled before starting work");
            isFetching.store(false);
            return;
        }
        
        // Perform the blocking HTTP request on this background thread
        std::string imageUrl = fetchImageUrl();
        
        // Check again after potentially long operation
        if (shouldStopThread.load()) {
            LOG_DEBUG("Fetch thread cancelled after fetchImageUrl");
            isFetching.store(false);
            return;
        }
        
        if (!imageUrl.empty()) {
            LOG_INFO("Successfully fetched image URL, starting image load");
            // Trigger async image load with fetched URL
            loadImageAsync(imageUrl, width, height);
            
            // Reset failure counter on successful fetch
            {
                std::lock_guard<std::mutex> lock(mutex);
                consecutiveFailures = 0;
            }
        } else {
            LOG_ERROR("Failed to fetch image URL");
            // Track failure for backoff
            time_t now = time(nullptr);
            {
                std::lock_guard<std::mutex> lock(mutex);
                consecutiveFailures++;
                lastFailedAttempt = now;
            }
            LOG_WARNING("Background fetch failed (attempt %d), will retry with backoff", 
                       consecutiveFailures);
        }
        
        isFetching.store(false);
        LOG_DEBUG("Fetch thread finished");
    });
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
    
    // CRITICAL: Join previous thread before starting a new one
    // This ensures we maintain ownership and prevent use-after-free
    if (backgroundThread.joinable()) {
        LOG_DEBUG("Joining previous background thread before starting new one");
        
        // Signal the thread to stop if it's taking too long
        time_t now = time(nullptr);
        time_t lastStart = lastThreadStart.load();
        if (lastStart > 0 && (now - lastStart) > THREAD_TIMEOUT) {
            LOG_WARNING("Previous background thread running for %ld seconds, signaling stop", now - lastStart);
            shouldStopThread.store(true);
        }
        
        // Always join - never detach
        backgroundThread.join();
        shouldStopThread.store(false); // Reset for next thread
        LOG_DEBUG("Previous background thread joined successfully");
    }
    
    LOG_INFO("Starting background image load from: %s", url.c_str());
    time_t now = time(nullptr);
    lastThreadStart.store(now);
    threadCount.fetch_add(1);
    isLoading.store(true);
    
    backgroundThread = std::thread([this, url, width, height]() {
        LOG_DEBUG("Background thread started");
        
        // Check if we should stop before doing work
        if (shouldStopThread.load()) {
            LOG_DEBUG("Background thread cancelled before starting work");
            isLoading.store(false);
            threadCount.fetch_sub(1);
            return;
        }
        
        SDL_Surface* newImage = loadImage(url, width, height);
        
        // Check again after potentially long operation
        if (shouldStopThread.load()) {
            LOG_DEBUG("Background thread cancelled after loadImage");
            if (newImage) {
                SDL_FreeSurface(newImage);
            }
            isLoading.store(false);
            threadCount.fetch_sub(1);
            return;
        }
        
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
        LOG_DEBUG("Background thread finished");
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
    
    // Check for hung thread - signal it to stop but don't detach
    time_t lastStart = lastThreadStart.load();
    if (isLoading.load() && lastStart > 0 && (currentTime - lastStart) > THREAD_TIMEOUT) {
        LOG_ERROR("Background thread appears hung (running for %ld seconds), signaling stop", 
                  currentTime - lastStart);
        
        // Signal the thread to stop cooperatively
        shouldStopThread.store(true);
        
        // Reset loading flag so we can try again
        // The hung thread will be joined on next loadImageAsync call or in destructor
        isLoading.store(false);
        
        // Don't reset threadCount - it will be decremented when thread finishes
    }
    
    // Check if we need to start loading a new image
    // CRITICAL: Also check !isFetching to prevent blocking the main thread
    if (!isLoading.load() && !isFetching.load() && 
        (difftime(currentTime, lastUpdate) > BACKGROUND_UPDATE_INTERVAL || currentImage == nullptr)) {
        
        // Calculate backoff delay based on consecutive failures (cap at 10 minutes)
        int backoffDelay = std::min(30 * consecutiveFailures, 600);
        
        // Check if enough time has passed since last failed attempt
        if (consecutiveFailures > 0 && difftime(currentTime, lastFailedAttempt) < backoffDelay) {
            // Still in backoff period, skip this attempt
            return;
        }
        
        // Update lastUpdate BEFORE attempting fetch to prevent infinite retries
        lastUpdate = currentTime;
        
        // Start async fetch - this returns immediately without blocking
        fetchImageUrlAsync(width, height);
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
        // This ensures the app remains functional when images are unavailable
        SDL_SetRenderDrawColor(renderer, FALLBACK_BG_RED, FALLBACK_BG_GREEN, FALLBACK_BG_BLUE, 255);
        SDL_RenderClear(renderer);
        
        // Log only once per startup or when transitioning to fallback
        static bool fallbackLogged = false;
        if (!fallbackLogged) {
            LOG_DEBUG("Using fallback background color (no image available yet)");
            fallbackLogged = true;
        }
    }
}