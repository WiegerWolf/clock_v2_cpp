#ifndef BACKGROUND_MANAGER_H
#define BACKGROUND_MANAGER_H

#include <string>
#include <SDL2/SDL.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <ctime>
#include <memory>

class HTTPClient;

class BackgroundManager {
public:
    BackgroundManager();
    ~BackgroundManager();

    void update(int width, int height);
    void draw(SDL_Renderer* renderer);
    std::string getError() const;

private:
    SDL_Surface* currentImage;
    SDL_Surface* overlay;
    SDL_Surface* pendingImage;
    
    // Surface ownership tracking
    bool pendingImageReady{false};  // Flag that pendingImage is ready to process
    std::atomic<bool> textureUpdateInProgress{false};
    
    // Cache the textures instead of recreating every frame
    SDL_Texture* currentTexture;
    SDL_Texture* overlayTexture;
    SDL_Renderer* cachedRenderer;
    
    time_t lastUpdate;
    std::atomic<time_t> lastFailedAttempt;
    std::atomic<int> consecutiveFailures;
    std::string error;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> isFetching{false};
    mutable std::mutex mutex;
    mutable std::mutex httpClientMutex;  // Separate mutex for HTTP client access
    std::thread backgroundThread;
    std::thread fetchThread;
    std::unique_ptr<HTTPClient> httpClient;  // Shared HTTPClient for circuit breaker persistence
    
    // NEW: Thread management
    std::atomic<bool> shouldStopThread{false};
    std::atomic<int> threadCount{0};
    static constexpr int MAX_THREADS = 1;
    
    // NEW: Timeout tracking
    std::atomic<time_t> lastThreadStart{0};
    static constexpr int THREAD_TIMEOUT = 60; // seconds

    std::string fetchImageUrl();
    void fetchImageUrlAsync(int width, int height);
    SDL_Surface* loadImage(const std::string& url, int width, int height);
    SDL_Surface* createDarkeningOverlay(int width, int height);
    void loadImageAsync(const std::string& url, int width, int height);
    void updateTextures(SDL_Renderer* renderer);
};

#endif