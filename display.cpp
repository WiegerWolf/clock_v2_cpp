// display.cpp
#include "display.h"
#include "config.h"
#include "constants.h"
#include "background_manager.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <climits>
#include <cstring>
#include <algorithm>

Display::Display(SDL_Renderer* renderTarget, int width, int height)
    : renderer(renderTarget), sizeW(width), sizeH(height), fontLarge(nullptr), fontSmall(nullptr),
    backgroundManager(new BackgroundManager()), textCapture(nullptr), textPixels(nullptr),
    textureChanged(false), previousTextPixels(nullptr), frameCounter(0), currentCacheMemory(0),
    currentFps(0.0f) {
    
    lastFrameTime = std::chrono::high_resolution_clock::now();
    
    screenSurface = SDL_CreateRGBSurface(0, sizeW, sizeH, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!screenSurface) {
        std::cerr << "SDL_CreateRGBSurface failed: " << SDL_GetError() << std::endl;
    }

    int fontSize = calculateFontSize();
    fontLarge = TTF_OpenFont(FONT_PATH, fontSize);
    if (!fontLarge) {
        std::cerr << "TTF_OpenFont (large) failed: " << TTF_GetError() << std::endl;
    }
    fontSmall = TTF_OpenFont(FONT_PATH, fontSize / 8);
    if (!fontSmall) {
        std::cerr << "TTF_OpenFont (small) failed: " << TTF_GetError() << std::endl;
    }

    mainTarget = SDL_GetRenderTarget(renderer);
    textCapture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET, width, height);
    if (!textCapture) {
        std::cerr << "Failed to create text capture texture: " << SDL_GetError() << std::endl;
    }
    texturePitch = width * 4;
    textPixels = new Uint32[width * height];
    previousTextPixels = new Uint32[width * height];
    std::memset(previousTextPixels, 0, width * height * sizeof(Uint32));
}

Display::~Display() {
    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (screenSurface) SDL_FreeSurface(screenSurface);
    if (backgroundManager) delete backgroundManager;
    if (textCapture) SDL_DestroyTexture(textCapture);
    delete[] textPixels;
    delete[] previousTextPixels;

    // Cleanup texture cache
    for (auto& pair : textureCache) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
}

size_t Display::estimateTextureMemory(int width, int height) {
    // Assuming RGBA8888 format (4 bytes per pixel)
    return width * height * 4;
}

void Display::removeOldestTexture() {
    if (textureCache.empty()) return;

    auto oldest = textureCache.begin();
    Uint32 oldestTime = oldest->second.lastUsed;

    for (auto it = textureCache.begin(); it != textureCache.end(); ++it) {
        if (it->second.lastUsed < oldestTime) {
            oldest = it;
            oldestTime = it->second.lastUsed;
        }
    }

    currentCacheMemory -= oldest->second.memorySize;
    SDL_DestroyTexture(oldest->second.texture);
    textureCache.erase(oldest);
}

SDL_Texture* Display::createTextTexture(const std::string& text, TTF_Font* font, SDL_Color color, SDL_Rect& outRect) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        std::cerr << "TTF_RenderUTF8_Blended failed: " << TTF_GetError() << std::endl;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return nullptr;
    }

    outRect.w = surface->w;
    outRect.h = surface->h;
    SDL_FreeSurface(surface);
    return texture;
}

SDL_Texture* Display::getCachedTexture(const std::string& text, TTF_Font* font, 
                                     SDL_Color color, bool isDynamic) {
    TextKey key{text, font, color, isDynamic};
    auto it = textureCache.find(key);
    
    if (it != textureCache.end()) {
        // Update last used time
        it->second.lastUsed = frameCounter;
        return it->second.texture;
    }

    // Create new texture
    SDL_Rect rect;
    SDL_Texture* texture = createTextTexture(text, font, color, rect);
    if (!texture) return nullptr;

    // Calculate memory size
    size_t memSize = estimateTextureMemory(rect.w, rect.h);

    // Manage cache size
    while ((textureCache.size() >= MAX_CACHE_SIZE || currentCacheMemory + memSize > MAX_CACHE_MEMORY) 
           && !textureCache.empty()) {
        removeOldestTexture();
    }

    // Add to cache
    currentCacheMemory += memSize;
    textureCache[key] = CachedTexture{
        texture,
        frameCounter,
        rect,
        memSize
    };

    return texture;
}

void Display::cleanupCache() {
    std::vector<TextKey> toRemove;
    
    for (const auto& pair : textureCache) {
        if (frameCounter - pair.second.lastUsed > CACHE_LIFETIME || 
            (pair.first.isDynamic && frameCounter - pair.second.lastUsed > 1)) {
            toRemove.push_back(pair.first);
        }
    }

    for (const auto& key : toRemove) {
        currentCacheMemory -= textureCache[key].memorySize;
        SDL_DestroyTexture(textureCache[key].texture);
        textureCache.erase(key);
    }
}

void Display::renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY, bool isDynamic) {
    // Get cached or create new texture
    SDL_Texture* texture = getCachedTexture(text, font, color, isDynamic);
    if (!texture) return;

    // Get the cached rect for size
    const SDL_Rect& cachedRect = textureCache[TextKey{text, font, color, isDynamic}].rect;
    
    // Calculate position
    SDL_Rect destRect = {
        centerX - cachedRect.w / 2,
        centerY - cachedRect.h / 2,
        cachedRect.w,
        cachedRect.h
    };

    // For dynamic text or if we need collision detection, render to text capture
    if (isDynamic) {
        SDL_SetRenderTarget(renderer, textCapture);
        SDL_RenderCopy(renderer, texture, nullptr, &destRect);
    }
    
    // Render to screen
    SDL_SetRenderTarget(renderer, mainTarget);
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);

    // Increment frame counter
    frameCounter++;
}

// Rest of the implementation remains largely unchanged
void Display::renderMultilineText(const std::string& text, TTF_Font* font, SDL_Color color, 
                                int centerX, int startY, float lineSpacing) {
    int maxWidth = static_cast<int>(sizeW * 0.9);
    
    if (!needsTwoLines(text, font, maxWidth)) {
        renderText(text, font, color, centerX, startY);
        return;
    }

    auto lines = splitIntoTwoLines(text);
    int lineHeight = static_cast<int>(TTF_FontLineSkip(font) * lineSpacing);

    renderText(lines.first, font, color, centerX, startY);
    renderText(lines.second, font, color, centerX, startY + lineHeight);
}

// Rest of the implementation (calculateFontSize, wrapText, etc.) remains unchanged
int Display::calculateFontSize() {
    int targetHeight = static_cast<int>(sizeH * 0.8);
    int testSize = 100;
    while (true) {
        TTF_Font* testFont = TTF_OpenFont(FONT_PATH, testSize);
        if (!testFont) return testSize - 10;
        int textWidth, textHeight;
        TTF_SizeText(testFont, "22:22", &textWidth, &textHeight);
        TTF_CloseFont(testFont);
        if (textHeight >= targetHeight || textWidth >= sizeW * 0.9) {
            return std::max(10, testSize - 10);
        }
        testSize += 10;
    }
}

std::vector<std::string> Display::wrapText(const std::string& text, TTF_Font* font, int maxWidth) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string word, currentLine;

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

    for (int i = std::max(0, midPoint - 2); i < std::min(totalWords, midPoint + 3); ++i) {
        std::string firstPart, secondPart;
        for(int j = 0; j < i; ++j) firstPart += words[j] + " ";
        for(int j = i; j < words.size(); ++j) secondPart += words[j] + " ";

        int diff = std::abs((int)firstPart.length() - (int)secondPart.length());
        if (diff < minDiff) {
            minDiff = diff;
            bestSplit = i;
        }
    }

    std::string line1, line2;
    for(int i = 0; i < bestSplit; ++i) line1 += words[i] + " ";
    for(int i = bestSplit; i < words.size(); ++i) line2 += words[i] + " ";
    return {line1, line2};
}

void Display::clear() {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
}

void Display::updateFpsCounter() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime);
    if (duration.count() > 0) {
        currentFps = 1000.0f / duration.count();
    }
    lastFrameTime = now;
}

void Display::renderFpsCounter() {
    std::string fpsText = std::to_string(static_cast<int>(currentFps)) + " FPS";
    SDL_Color white = {255, 255, 255, 255};
    
    // Position in top-right corner with 10px padding
    int textWidth, textHeight;
    TTF_SizeText(fontSmall, fpsText.c_str(), &textWidth, &textHeight);
    int x = sizeW - textWidth - 10;
    int y = 10;
    
    renderText(fpsText, fontSmall, white, x + textWidth/2, y + textHeight/2, true);
}

void Display::update() {
    updateFpsCounter();
    renderFpsCounter();
    // SDL_Renderer is updated in the main loop after drawing everything
}

void Display::beginTextCapture() {
    mainTarget = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, textCapture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
}

void Display::endTextCapture() {
    updateTextCapture();
    SDL_SetRenderTarget(renderer, mainTarget);
}

void Display::updateTextCapture() {
    SDL_Rect rect = {0, 0, sizeW, sizeH};
    if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGBA8888,
                            textPixels, texturePitch) < 0) {
        std::cerr << "Failed to read pixels: " << SDL_GetError() << std::endl;
    }
    checkTextureChange();
}

void Display::checkTextureChange() {
    textureChanged = false;
    for (int i = 0; i < sizeW * sizeH; i++) {
        if (textPixels[i] != previousTextPixels[i]) {
            textureChanged = true;
            break;
        }
    }
    std::memcpy(previousTextPixels, textPixels, sizeW * sizeH * sizeof(Uint32));
}

bool Display::isPixelOccupied(int x, int y) const {
    if (x < 0 || x >= sizeW || y < 0 || y >= sizeH) return false;
    Uint32 pixel = textPixels[y * sizeW + x];
    Uint8 alpha = (pixel & 0xFF000000) >> 24;
    return alpha > 50;
}