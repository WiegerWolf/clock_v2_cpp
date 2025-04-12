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
    std::string getError() const; // Make const version lock

private:
    std::string fetchImageUrl();
    SDL_Surface* createDarkeningOverlay(int width, int height);
    SDL_Surface* loadImage(const std::string& url, int width, int height);
    void loadImageAsync(const std::string& url, int width, int height);

    SDL_Surface* currentImage;
    SDL_Surface* overlay;
    time_t lastUpdate;
    std::string error;
    mutable std::mutex mutex; // Make mutex mutable
    std::atomic<bool> isLoading{false};
    SDL_Surface* pendingImage;
};

#endif // BACKGROUND_MANAGER_H
