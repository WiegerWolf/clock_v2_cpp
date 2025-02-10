// display.cpp
#include "display.h"
#include "config.h"
#include "constants.h"
#include "background_manager.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <climits>  // Add this line for INT_MAX
#include <cstring>  // Add this for memset and memcpy

Display::Display(SDL_Renderer* renderTarget, int width, int height)
    : renderer(renderTarget), sizeW(width), sizeH(height), fontLarge(nullptr), fontSmall(nullptr), 
    backgroundManager(new BackgroundManager()), textCapture(nullptr), textPixels(nullptr),
    textureChanged(false), previousTextPixels(nullptr), mainTarget(nullptr), screenSurface(nullptr),
    batchTexture(nullptr), texturePitch(0) {
    
    if (!renderer) {
        throw std::runtime_error("Null renderer passed to Display constructor");
    }

    mainTarget = SDL_GetRenderTarget(renderer);

    screenSurface = SDL_CreateRGBSurface(0, sizeW, sizeH, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!screenSurface) {
        throw std::runtime_error("SDL_CreateRGBSurface failed: " + std::string(SDL_GetError()));
    }

    int fontSize = calculateFontSize();
    fontLarge = TTF_OpenFont(FONT_PATH, fontSize);
    if (!fontLarge) {
        SDL_FreeSurface(screenSurface);
        throw std::runtime_error("TTF_OpenFont (large) failed: " + std::string(TTF_GetError()));
    }

    fontSmall = TTF_OpenFont(FONT_PATH, fontSize / 8);
    if (!fontSmall) {
        SDL_FreeSurface(screenSurface);
        TTF_CloseFont(fontLarge);
        throw std::runtime_error("TTF_OpenFont (small) failed: " + std::string(TTF_GetError()));
    }

    texturePitch = width * 4; // 4 bytes per pixel (RGBA)
    
    // Allocate and initialize pixel buffers
    try {
        textPixels = new Uint32[width * height];
        previousTextPixels = new Uint32[width * height];
    } catch (const std::bad_alloc& e) {
        SDL_FreeSurface(screenSurface);
        TTF_CloseFont(fontLarge);
        TTF_CloseFont(fontSmall);
        throw std::runtime_error("Failed to allocate pixel buffers");
    }
    
    std::memset(textPixels, 0, width * height * sizeof(Uint32));
    std::memset(previousTextPixels, 0, width * height * sizeof(Uint32));

    // Create text capture texture with proper streaming access
    textCapture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_STREAMING,  // Remove SDL_TEXTUREACCESS_TARGET
                                  width, height);
    if (!textCapture) {
        cleanup();
        throw std::runtime_error("Failed to create text capture texture: " + std::string(SDL_GetError()));
    }

    // Initialize textCapture with transparency using the proper method for streaming textures
    void* pixels;
    int pitch;
    if (SDL_LockTexture(textCapture, nullptr, &pixels, &pitch) == 0) {
        std::memset(pixels, 0, height * pitch);
        SDL_UnlockTexture(textCapture);
    }
}

void Display::cleanup() {
    if (textPixels) {
        delete[] textPixels;
        textPixels = nullptr;
    }
    if (previousTextPixels) {
        delete[] previousTextPixels;
        previousTextPixels = nullptr;
    }
    if (screenSurface) {
        SDL_FreeSurface(screenSurface);
        screenSurface = nullptr;
    }
    if (fontLarge) {
        TTF_CloseFont(fontLarge);
        fontLarge = nullptr;
    }
    if (fontSmall) {
        TTF_CloseFont(fontSmall);
        fontSmall = nullptr;
    }
}

Display::~Display() {
    for (auto& pair : textureCache) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
    textureCache.clear();
    
    if (textCapture) {
        SDL_DestroyTexture(textCapture);
    }
    if (batchTexture) {
        SDL_DestroyTexture(batchTexture);
    }
    if (backgroundManager) {
        delete backgroundManager;
    }
    
    cleanup();
}

int Display::calculateFontSize() {
    int targetHeight = static_cast<int>(sizeH * 0.8);
    int testSize = 100;
    while (true) {
        TTF_Font* testFont = TTF_OpenFont(FONT_PATH, testSize);
        if (!testFont) return testSize - 10; // Error opening font, return a slightly smaller size
        int textWidth, textHeight;
        TTF_SizeText(testFont, "22:22", &textWidth, &textHeight);
        TTF_CloseFont(testFont);
        if (textHeight >= targetHeight || textWidth >= sizeW * 0.9) {
            return std::max(10, testSize - 10); // Ensure minimum font size and step back.
        }
        testSize += 10;
    }
}

std::vector<std::string> Display::wrapText(const std::string& text, TTF_Font* font, int maxWidth) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string word;
    std::string currentLine;

    while (ss >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        int textWidth, textHeight;
        TTF_SizeText(font, testLine.c_str(), &textWidth, &textHeight);
        if (textWidth <= maxWidth) {
            currentLine = testLine;
        } else {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
            }
            currentLine = word;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }
    return lines;
}

bool Display::needsTwoLines(const std::string& text, TTF_Font* font, int maxWidth) {
    int textWidth, textHeight;
    TTF_SizeText(font, text.c_str(), &textWidth, &textHeight);
    return textWidth > maxWidth;
}

std::pair<std::string, std::string> Display::splitIntoTwoLines(const std::string& text) {
    std::stringstream ss(text);
    std::vector<std::string> words;
    std::string word;
    while (ss >> word) {
        words.push_back(word);
    }
    int totalWords = words.size();
    int midPoint = totalWords / 2;
    int bestSplit = midPoint;
    int minDiff = INT_MAX;

    for (int i = std::max(0, midPoint - 2); i < std::min((int)words.size(), midPoint + 3); ++i) {
        std::string firstPart, secondPart;
        for(int j=0; j<i; ++j) firstPart += words[j] + " ";
        for(int j=i; j<words.size(); ++j) secondPart += words[j] + " ";

        int diff = std::abs((int)firstPart.length() - (int)secondPart.length());
        if (diff < minDiff) {
            minDiff = diff;
            bestSplit = i;
        }
    }
    std::string line1, line2;
    for(int i=0; i<bestSplit; ++i) line1 += words[i] + " ";
    for(int i=bestSplit; i<words.size(); ++i) line2 += words[i] + " ";
    return {line1, line2};
}

void Display::renderMultilineText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int startY, float lineSpacing) {
    int maxWidth = static_cast<int>(sizeW * 0.9);
    int maxHeight = sizeH - startY;
    int currentSize = TTF_FontHeight(font);

    if (!needsTwoLines(text, font, maxWidth)) {
        renderText(text, font, color, centerX, startY);
        return;
    }

    std::pair<std::string, std::string> lines = splitIntoTwoLines(text);
    std::string line1 = lines.first;
    std::string line2 = lines.second;

    int minSize = 8;
    int maxSize = currentSize;
    int optimalSize = minSize;

    while (minSize <= maxSize) {
        int testSize = (minSize + maxSize) / 2;
        TTF_Font* testFont = TTF_OpenFont(FONT_PATH, testSize);
        if (!testFont) break; // Handle font opening error

        int width1, height1, width2, height2;
        TTF_SizeText(testFont, line1.c_str(), &width1, &height1);
        TTF_SizeText(testFont, line2.c_str(), &width2, &height2);
        int totalHeight = static_cast<int>(TTF_FontLineSkip(testFont) * lineSpacing * 2);

        if (width1 <= maxWidth && width2 <= maxWidth && totalHeight <= maxHeight) {
            optimalSize = testSize;
            minSize = testSize + 1;
        } else {
            maxSize = testSize - 1;
        }
        TTF_CloseFont(testFont);
    }

    TTF_Font* finalFont = TTF_OpenFont(FONT_PATH, optimalSize);
    if (!finalFont) return; // Handle font opening error

    int lineHeight = static_cast<int>(TTF_FontLineSkip(finalFont) * lineSpacing);
    renderText(line1, finalFont, color, centerX, startY);
    renderText(line2, finalFont, color, centerX, startY + lineHeight);
    TTF_CloseFont(finalFont);
}

SDL_Texture* Display::getCachedTexture(const std::string& text, TTF_Font* font, SDL_Color color, SDL_Rect& outRect) {
    Uint32 currentTime = SDL_GetTicks();
    
    // Create a unique key for this text/font combination
    std::string key = text + "_" + std::to_string(TTF_FontHeight(font));
    
    // Check if we have a cached version
    auto it = textureCache.find(key);
    if (it != textureCache.end()) {
        it->second.lastUsed = currentTime;
        outRect = it->second.rect;
        return it->second.texture;
    }
    
    // Create new texture
    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!textSurface) return nullptr;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!texture) {
        SDL_FreeSurface(textSurface);
        return nullptr;
    }
    
    // Cache the new texture
    TextureCache cache;
    cache.texture = texture;
    cache.rect = {0, 0, textSurface->w, textSurface->h};
    cache.text = text;
    cache.fontSize = TTF_FontHeight(font);
    cache.lastUsed = currentTime;
    
    outRect = cache.rect;
    
    // Clean up old textures if cache is too large
    if (textureCache.size() >= MAX_CACHE_SIZE) {
        cleanupOldCachedTextures(currentTime);
    }
    
    textureCache[key] = cache;
    SDL_FreeSurface(textSurface);
    return texture;
}

void Display::cleanupOldCachedTextures(Uint32 currentTime) {
    std::vector<std::string> keysToRemove;
    
    for (const auto& pair : textureCache) {
        if (currentTime - pair.second.lastUsed > TEXTURE_CACHE_LIFETIME) {
            keysToRemove.push_back(pair.first);
        }
    }
    
    for (const auto& key : keysToRemove) {
        SDL_DestroyTexture(textureCache[key].texture);
        textureCache.erase(key);
    }
}

void Display::renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY) {
    SDL_Rect textRect;
    SDL_Texture* textTexture = getCachedTexture(text, font, color, textRect);
    if (!textTexture) return;

    textRect.x = centerX - textRect.w / 2;
    textRect.y = centerY - textRect.h / 2;

    // Draw directly to current target
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

    // Create or update batch texture if needed
    if (!batchTexture) {
        batchTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, sizeW, sizeH);
        if (!batchTexture) {
            std::cerr << "Failed to create batch texture: " << SDL_GetError() << std::endl;
            return;
        }
        // Initialize with transparency
        SDL_SetRenderTarget(renderer, batchTexture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, mainTarget);
    }

    // Update collision texture
    SDL_SetRenderTarget(renderer, batchTexture);
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_SetRenderTarget(renderer, mainTarget);
}

void Display::clear() {
    // Remove background management code, only handle text rendering
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
}

void Display::update() {
    // SDL_Renderer is updated in the main loop after drawing everything.
}

void Display::beginTextCapture() {
    SDL_SetRenderTarget(renderer, textCapture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
}

void Display::endTextCapture() {
    updateTextCapture();
    SDL_SetRenderTarget(renderer, mainTarget);
}

void Display::updateTextCapture() {
    static Uint32 lastCheckTime = 0;
    Uint32 currentTime = SDL_GetTicks();
    
    // Only check for texture changes every 32ms (30fps is enough for collision detection)
    if (currentTime - lastCheckTime < 32) {
        return;
    }
    lastCheckTime = currentTime;

    // Use direct pixel access for better performance
    void* pixels;
    int pitch;
    if (SDL_LockTexture(textCapture, NULL, &pixels, &pitch) < 0) {
        std::cerr << "Failed to lock texture: " << SDL_GetError() << std::endl;
        return;
    }

    if (pixels) {
        size_t size = sizeW * sizeH * sizeof(Uint32);
        #pragma omp parallel for if(size > 1024*1024)
        for (int y = 0; y < sizeH; y++) {
            memcpy(
                static_cast<Uint32*>(textPixels) + y * sizeW,
                static_cast<Uint8*>(pixels) + y * pitch,
                sizeW * sizeof(Uint32)
            );
        }
        SDL_UnlockTexture(textCapture);
        checkTextureChange();
    } else {
        SDL_UnlockTexture(textCapture);
    }
}

void Display::checkTextureChange() {
    textureChanged = false;
    
    // Compare in chunks of 64 bytes for better cache utilization
    const uint64_t* curr = reinterpret_cast<const uint64_t*>(textPixels);
    const uint64_t* prev = reinterpret_cast<const uint64_t*>(previousTextPixels);
    const size_t numWords = (sizeW * sizeH * sizeof(Uint32)) / sizeof(uint64_t);
    
    #pragma omp parallel for reduction(||:textureChanged)
    for (size_t i = 0; i < numWords; i += 8) {
        textureChanged = textureChanged || 
            curr[i] != prev[i] ||
            curr[i+1] != prev[i+1] ||
            curr[i+2] != prev[i+2] ||
            curr[i+3] != prev[i+3] ||
            curr[i+4] != prev[i+4] ||
            curr[i+5] != prev[i+5] ||
            curr[i+6] != prev[i+6] ||
            curr[i+7] != prev[i+7];
    }
    
    if (textureChanged) {
        memcpy(previousTextPixels, textPixels, sizeW * sizeH * sizeof(Uint32));
    }
}

bool Display::isPixelOccupied(int x, int y) const {
    if (x < 0 || x >= sizeW || y < 0 || y >= sizeH) return false;
    Uint32 pixel = textPixels[y * sizeW + x];
    return (pixel & 0xFF000000) > 0x32000000; // Alpha > 50
}