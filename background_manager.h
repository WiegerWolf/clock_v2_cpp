#ifndef BACKGROUND_MANAGER_H
#define BACKGROUND_MANAGER_H

#include <string>
#include <SDL2/SDL.h>
#include <mutex>
#include <thread>
#include <atomic>

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
    
    // Cache the textures instead of recreating every frame
    SDL_Texture* currentTexture;
    SDL_Texture* overlayTexture;
    SDL_Renderer* cachedRenderer;
    
    time_t lastUpdate;
    std::string error;
    std::atomic<bool> isLoading{false};
    mutable std::mutex mutex;
    std::thread backgroundThread;

    std::string fetchImageUrl();
    SDL_Surface* loadImage(const std::string& url, int width, int height);
    SDL_Surface* createDarkeningOverlay(int width, int height);
    void loadImageAsync(const std::string& url, int width, int height);
    void updateTextures(SDL_Renderer* renderer);
};

#endif