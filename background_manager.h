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
    std::atomic<time_t> lastFailedAttempt{0};
    std::atomic<int> consecutiveFailures{0};
    std::string error;
    std::atomic<bool> isLoading{false};
    mutable std::mutex mutex;
    mutable std::mutex httpClientMutex;  // Separate mutex for HTTP client access
    
    // Simplified threading: Single worker thread
    std::thread workerThread;
    std::unique_ptr<HTTPClient> httpClient;
    
    // Thread management
    std::atomic<bool> shouldStopThread{false};
    std::atomic<time_t> lastThreadStart{0};
    static constexpr int THREAD_TIMEOUT = 60; // seconds

    std::string fetchImageUrl();
    SDL_Surface* loadImage(const std::string& url, int width, int height);
    SDL_Surface* createDarkeningOverlay(int width, int height);
    void startBackgroundUpdate(int width, int height);
    void updateTextures(SDL_Renderer* renderer);
};

#endif