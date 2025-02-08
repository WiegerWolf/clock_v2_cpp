// background_manager.cpp
#define CPPHTTPLIB_OPENSSL_SUPPORT
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
    
    std::cout << "Fetching from: " << BACKGROUND_API_URL_HOST << BACKGROUND_API_URL_PATH << std::endl;
    auto res = cli.Get(BACKGROUND_API_URL_PATH);
    
    if (!res) {
        error = "Failed to get response";
        std::cout << "HTTP request failed: no response" << std::endl;
        return "";
    }
    
    std::cout << "Status: " << res->status << std::endl;
    std::cout << "Response body: " << res->body << std::endl;
    
    if (res->status == 200) {
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
    error = "HTTP request failed: " + std::to_string(res->status);
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
    std::cout << "Loading image from URL: " << url << std::endl;
    
    // Parse URL properly - remove https:// prefix
    std::string hostWithProto = url.substr(0, url.find("/", 8));
    std::string host = hostWithProto.substr(hostWithProto.find("://") + 3);
    std::string path = url.substr(url.find("/", 8));
    
    std::cout << "Host (cleaned): " << host << std::endl;
    std::cout << "Path: " << path << std::endl;
    
    httplib::SSLClient cli(host);
    cli.set_follow_location(true);
    cli.enable_server_certificate_verification(false);
    
    // Set read timeout to avoid hanging
    cli.set_read_timeout(5, 0); // 5 seconds timeout
    cli.set_write_timeout(5, 0);
    cli.set_connection_timeout(5, 0);
    
    auto res = cli.Get(path.c_str());
    
    if (!res) {
        error = "Failed to get image response: " + std::string(httplib::to_string(res.error()));
        std::cout << "Image HTTP request failed: " << error << std::endl;
        return nullptr;
    }
    
    std::cout << "Image response status: " << res->status << std::endl;
    std::cout << "Image response size: " << res->body.size() << std::endl;
    
    if (res && res->status == 200) {
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
    error = "HTTP image request failed: " + std::to_string(res ? res->status : -1);
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