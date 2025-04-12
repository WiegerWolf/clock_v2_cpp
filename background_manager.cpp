// background_manager.cpp
#include "background_manager.h"
#include "config.h"
#include "constants.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <httplib.h>
#include <SDL_image.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

BackgroundManager::BackgroundManager() : currentImage(nullptr), overlay(nullptr), lastUpdate(0), error(""), pendingImage(nullptr) {}

// Implement the const getError with locking
std::string BackgroundManager::getError() const {
    std::lock_guard<std::mutex> lock(mutex);
    return error;
}

BackgroundManager::~BackgroundManager() {
    if (currentImage) {
        SDL_FreeSurface(currentImage);
    }
    if (overlay) {
        SDL_FreeSurface(overlay);
    }
}

std::string BackgroundManager::fetchImageUrl() {
    httplib::SSLClient cli(BACKGROUND_API_URL_HOST, BACKGROUND_API_URL_PORT);
    cli.set_follow_location(true);
    cli.enable_server_certificate_verification(false);
    
    auto res = cli.Get(BACKGROUND_API_URL_PATH);
    if (!res) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            error = "Failed to get response";
        }
        return "";
    }
    
    if (res->status == 200) {
        try {
            json data = json::parse(res->body);
            return data[0]["fullUrl"].get<std::string>();
        } catch (const json::parse_error& e) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                error = "JSON parse error: " + std::string(e.what());
            }
            return "";
        } catch (const std::exception& e) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                error = "Error processing JSON: " + std::string(e.what());
            }
            return "";
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        error = "HTTP request failed: " + std::to_string(res->status);
    }
    return "";
}

SDL_Surface* BackgroundManager::createDarkeningOverlay(int width, int height) {
    SDL_Surface* overlaySurface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!overlaySurface) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            error = "SDL_CreateRGBSurface failed: " + std::string(SDL_GetError());
        }
        return nullptr;
    }
    SDL_FillRect(overlaySurface, NULL, SDL_MapRGBA(overlaySurface->format, 0, 0, 0, static_cast<int>(255 * BACKGROUND_DARKNESS)));
    return overlaySurface;
}

SDL_Surface* BackgroundManager::loadImage(const std::string& url, int width, int height) {
    std::string hostWithProto = url.substr(0, url.find("/", 8));
    std::string host = hostWithProto.substr(hostWithProto.find("://") + 3);
    std::string path = url.substr(url.find("/", 8));
    
    httplib::SSLClient cli(host);
    cli.set_follow_location(true);
    cli.enable_server_certificate_verification(false);
    
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);
    cli.set_connection_timeout(5, 0);
    
    auto res = cli.Get(path.c_str());
    if (!res) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            error = "Failed to get image response: " + std::string(httplib::to_string(res.error()));
        }
        return nullptr;
    }
    
    if (res && res->status == 200) {
        SDL_RWops* rw = SDL_RWFromMem((void*)res->body.data(), res->body.size());
        if (!rw) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                error = "SDL_RWFromMem failed: " + std::string(SDL_GetError());
            }
            return nullptr;
        }
        SDL_Surface* imageSurface = IMG_Load_RW(rw, 1);
        if (!imageSurface) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                error = "IMG_Load_RW failed: " + std::string(IMG_GetError());
            }
            // SDL_RWclose(rw); // Should be closed here too
            return nullptr;
        }
        // SDL_RWclose(rw); // Close RWops after use
        SDL_Surface* scaledSurface = SDL_CreateRGBSurface(0, width, height, 32, imageSurface->format->Rmask, imageSurface->format->Gmask, imageSurface->format->Bmask, imageSurface->format->Amask);
        if (!scaledSurface) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                error = "SDL_CreateRGBSurface (scaled) failed: " + std::string(SDL_GetError());
            }
            SDL_FreeSurface(imageSurface);
            return nullptr;
        }
        if (SDL_BlitScaled(imageSurface, NULL, scaledSurface, NULL) < 0) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                error = "SDL_BlitScaled failed: " + std::string(SDL_GetError());
            }
            SDL_FreeSurface(imageSurface);
            SDL_FreeSurface(scaledSurface);
            return nullptr;
        }
        SDL_FreeSurface(imageSurface);
        overlay = createDarkeningOverlay(width, height); // Potential error write inside this needs lock too (already added)
        return scaledSurface;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        error = "HTTP image request failed: " + std::to_string(res ? res->status : -1);
    }
    return nullptr;
}

void BackgroundManager::loadImageAsync(const std::string& url, int width, int height) {
    if (isLoading) return;
    
    isLoading = true;
    std::thread([this, url, width, height]() {
        SDL_Surface* newImage = loadImage(url, width, height);
        if (newImage) {
            std::lock_guard<std::mutex> lock(mutex);
            pendingImage = newImage;
        }
        isLoading = false;
    }).detach();
}

void BackgroundManager::update(int width, int height) {
    time_t currentTime;
    time(&currentTime);
    
    // Handle pending image
    if (pendingImage) {
        std::lock_guard<std::mutex> lock(mutex);
        if (currentImage) {
            SDL_FreeSurface(currentImage);
        }
        currentImage = pendingImage;
        pendingImage = nullptr;
        lastUpdate = currentTime;
    }
    
    // Check if we need to start loading a new image
    if (!isLoading && (difftime(currentTime, lastUpdate) > BACKGROUND_UPDATE_INTERVAL || currentImage == nullptr)) {
        std::string imageUrl = fetchImageUrl();
        if (!imageUrl.empty()) {
            loadImageAsync(imageUrl, width, height);
        }
    }
}

void BackgroundManager::draw(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(mutex);
    // Remove SDL_RenderClear here since it's handled in Clock::draw
    if (currentImage) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, currentImage);
        if (texture) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_DestroyTexture(texture);
        } else {
            // Lock not strictly needed here if error is only read by getError(),
            // but good practice to lock writes consistently.
            // std::lock_guard<std::mutex> lock(mutex); // Optional lock for write consistency
            // error = "SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError());
            // Let's skip locking this one as it's less critical and only happens in main thread draw path
             error = "SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError());
        }
    }
    if (overlay) {
        SDL_Texture* overlayTexture = SDL_CreateTextureFromSurface(renderer, overlay);
        if (overlayTexture) {
            SDL_RenderCopy(renderer, overlayTexture, NULL, NULL);
            SDL_DestroyTexture(overlayTexture);
        } else {
            // Skip locking this one too for same reason as above
            error = "SDL_CreateTextureFromSurface (overlay) failed: " + std::string(SDL_GetError());
        }
    }
}
