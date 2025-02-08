// background_manager.cpp
#include "background_manager.h"
#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <httplib.h>
#include <SDL_image.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

BackgroundManager::BackgroundManager() : currentImage(nullptr), overlay(nullptr), lastUpdate(0), error("") {}

BackgroundManager::~BackgroundManager() {
    if (currentImage) {
        SDL_FreeSurface(currentImage);
    }
    if (overlay) {
        SDL_FreeSurface(overlay);
    }
}

std::string BackgroundManager::fetchImageUrl() {
    httplib::Client cli(BACKGROUND_API_URL_HOST, BACKGROUND_API_URL_PORT);
    auto res = cli.Get(BACKGROUND_API_URL_PATH);
    if (res && res->status == 200) {  // Changed from status() to status
        try {
            json data = json::parse(res->body);
            return data[0]["fullUrl"].get<std::string>();
        } catch (const json::parse_error& e) {
            error = "JSON parse error: " + std::string(e.what());
            return "";
        } catch (const std::exception& e) {
            error = "Error processing JSON: " + std::string(e.what());
            return "";
        }
    }
    error = "HTTP request failed: " + std::to_string(res ? res->status : -1);  // Changed from status() to status
    return "";
}

SDL_Surface* BackgroundManager::createDarkeningOverlay(int width, int height) {
    SDL_Surface* overlaySurface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!overlaySurface) {
        error = "SDL_CreateRGBSurface failed: " + std::string(SDL_GetError());
        return nullptr;
    }
    SDL_FillRect(overlaySurface, NULL, SDL_MapRGBA(overlaySurface->format, 0, 0, 0, static_cast<int>(255 * BACKGROUND_DARKNESS)));
    return overlaySurface;
}

SDL_Surface* BackgroundManager::loadImage(const std::string& url, int width, int height) {
    httplib::Client cli(url.substr(0, url.find("/", 8)).c_str()); // Extract host from URL
    auto res = cli.Get(url.substr(url.find("/", 8)).c_str()); // Extract path from URL
    if (res && res->status == 200) {  // Changed from status() to status
        SDL_RWops* rw = SDL_RWFromMem((void*)res->body.data(), res->body.size());
        if (!rw) {
            error = "SDL_RWFromMem failed: " + std::string(SDL_GetError());
            return nullptr;
        }
        SDL_Surface* imageSurface = IMG_Load_RW(rw, 1);
        if (!imageSurface) {
            error = "IMG_Load_RW failed: " + std::string(IMG_GetError());
            return nullptr;
        }
        SDL_Surface* scaledSurface = SDL_CreateRGBSurface(0, width, height, 32, imageSurface->format->Rmask, imageSurface->format->Gmask, imageSurface->format->Bmask, imageSurface->format->Amask);
        if (!scaledSurface) {
            error = "SDL_CreateRGBSurface (scaled) failed: " + std::string(SDL_GetError());
            SDL_FreeSurface(imageSurface);
            return nullptr;
        }
        if (SDL_BlitScaled(imageSurface, NULL, scaledSurface, NULL) < 0) {
            error = "SDL_BlitScaled failed: " + std::string(SDL_GetError());
            SDL_FreeSurface(imageSurface);
            SDL_FreeSurface(scaledSurface);
            return nullptr;
        }
        SDL_FreeSurface(imageSurface);
        overlay = createDarkeningOverlay(width, height);
        return scaledSurface;
    }
    error = "HTTP image request failed: " + std::to_string(res ? res->status : -1);  // Changed from status() to status
    return nullptr;
}

void BackgroundManager::update(int width, int height) {
    time_t currentTime;
    time(&currentTime);
    if (difftime(currentTime, lastUpdate) > BACKGROUND_UPDATE_INTERVAL || currentImage == nullptr) {
        std::string imageUrl = fetchImageUrl();
        if (!imageUrl.empty()) {
            SDL_Surface* newImage = loadImage(imageUrl, width, height);
            if (newImage) {
                if (currentImage) {
                    SDL_FreeSurface(currentImage);
                }
                currentImage = newImage;
                lastUpdate = currentTime;
            }
        }
    }
}

void BackgroundManager::draw(SDL_Renderer* renderer) {
    if (currentImage) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, currentImage);
        if (texture) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_DestroyTexture(texture);
        } else {
            error = "SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError());
        }
    }
    if (overlay) {
        SDL_Texture* overlayTexture = SDL_CreateTextureFromSurface(renderer, overlay);
        if (overlayTexture) {
            SDL_RenderCopy(renderer, overlayTexture, NULL, NULL);
            SDL_DestroyTexture(overlayTexture);
        } else {
            error = "SDL_CreateTextureFromSurface (overlay) failed: " + std::string(SDL_GetError());
        }
    }
}