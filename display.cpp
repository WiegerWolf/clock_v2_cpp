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
    textureChanged(false), previousTextPixels(nullptr), mainTarget(nullptr), screenSurface(nullptr) {
    
    if (!renderer) {
        throw std::runtime_error("Null renderer passed to Display constructor");
    }

    // Get the current render target, if any
    mainTarget = SDL_GetRenderTarget(renderer);
    // Note: If no target is set, mainTarget will be nullptr, which is what we want
    
    screenSurface = SDL_CreateRGBSurface(0, sizeW, sizeH, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!screenSurface) {
        throw std::runtime_error("SDL_CreateRGBSurface failed: " + std::string(SDL_GetError()));
    }

    int fontSize = calculateFontSize();
    fontLarge = TTF_OpenFont(FONT_PATH, fontSize);
    if (!fontLarge) {
        throw std::runtime_error("TTF_OpenFont (large) failed: " + std::string(TTF_GetError()));
    }

    fontSmall = TTF_OpenFont(FONT_PATH, fontSize / 8);
    if (!fontSmall) {
        TTF_CloseFont(fontLarge);
        throw std::runtime_error("TTF_OpenFont (small) failed: " + std::string(TTF_GetError()));
    }

    textCapture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET | SDL_TEXTUREACCESS_STREAMING,
                                  width, height);
    if (!textCapture) {
        TTF_CloseFont(fontLarge);
        TTF_CloseFont(fontSmall);
        throw std::runtime_error("Failed to create text capture texture: " + std::string(SDL_GetError()));
    }

    texturePitch = width * 4; // 4 bytes per pixel (RGBA)
    textPixels = new Uint32[width * height];
    previousTextPixels = new Uint32[width * height];
    std::memset(previousTextPixels, 0, width * height * sizeof(Uint32));
}

Display::~Display() {
    for (auto& pair : textureCache) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (screenSurface) SDL_FreeSurface(screenSurface);
    if (backgroundManager) delete backgroundManager;
    if (textCapture) SDL_DestroyTexture(textCapture);
    delete[] textPixels;
    delete[] previousTextPixels;
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
    static SDL_Texture* batchTexture = nullptr;
    SDL_Rect textRect;
    SDL_Texture* textTexture = getCachedTexture(text, font, color, textRect);
    if (!textTexture) return;

    textRect.x = centerX - textRect.w / 2;
    textRect.y = centerY - textRect.h / 2;

    // Draw directly to current target to reduce switches
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

    // Update collision texture in background
    if (!batchTexture) {
        batchTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, sizeW, sizeH);
    }
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

    // Use streaming texture access for faster pixel operations
    void* pixels;
    int pitch;
    SDL_LockTexture(textCapture, NULL, &pixels, &pitch);
    memcpy(textPixels, pixels, sizeW * sizeH * sizeof(Uint32));
    SDL_UnlockTexture(textCapture);
    
    checkTextureChange();
}

void Display::checkTextureChange() {
    textureChanged = false;
    
    // Compare in chunks of 64 bytes for better performance
    const uint64_t* curr = reinterpret_cast<const uint64_t*>(textPixels);
    const uint64_t* prev = reinterpret_cast<const uint64_t*>(previousTextPixels);
    const size_t numWords = (sizeW * sizeH * sizeof(Uint32)) / sizeof(uint64_t);
    
    // Fast path: compare 64 bits at a time
    for (size_t i = 0; i < numWords; i++) {
        if (curr[i] != prev[i]) {
            textureChanged = true;
            break;
        }
    }
    
    if (textureChanged) {
        memcpy(previousTextPixels, textPixels, sizeW * sizeH * sizeof(Uint32));
    }
}

bool Display::isPixelOccupied(int x, int y) const {
    if (x < 0 || x >= sizeW || y < 0 || y >= sizeH) return false;
    Uint32 pixel = textPixels[y * sizeW + x];
    Uint8 alpha = (pixel & 0xFF000000) >> 24;
    return alpha > 50; // Consider pixels with alpha > 50 as solid
}