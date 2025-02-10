// background_manager.h
#ifndef BACKGROUND_MANAGER_H
#define BACKGROUND_MANAGER_H

#include <string>
#include <SDL.h>
#include <mutex>
#include <thread>
#include <atomic>

class BackgroundManager {
public:
    BackgroundManager();
    ~BackgroundManager();
    void update(int width, int height);
    void draw(SDL_Renderer* renderer);
    void render(SDL_Renderer* renderer);  // Added new optimized render method
    std::string getError() const { return error; }

private:
    std::string fetchImageUrl();
    SDL_Surface* createDarkeningOverlay(int width, int height);
    SDL_Surface* loadImage(const std::string& url, int width, int height);
    void loadImageAsync(const std::string& url, int width, int height);
    void createTextures(SDL_Renderer* renderer);
    void updateGradient();  // Added new gradient update method
    
    SDL_Surface* currentImage;
    SDL_Surface* overlay;
    SDL_Texture* currentTexture;  // Cached texture
    SDL_Texture* overlayTexture;  // Cached overlay texture
    SDL_Texture* backgroundTexture;  // Added for gradient
    time_t lastUpdate;
    std::string error;
    std::mutex mutex;
    std::atomic<bool> isLoading{false};
    SDL_Surface* pendingImage;
    bool texturesNeedUpdate;
    
    struct Color {
        Uint8 r, g, b;
    };
    Color topColor{0, 0, 139};     // Dark blue
    Color bottomColor{25, 25, 112}; // Midnight blue
};

#endif // BACKGROUND_MANAGER_H