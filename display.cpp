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
#include <cmath> // Needed for sin and cos

Display::Display(SDL_Renderer* renderTarget, int width, int height)
    : renderer(renderTarget), sizeW(width), sizeH(height), fontLarge(nullptr), fontSmall(nullptr), fontExtraSmall(nullptr), backgroundManager(new BackgroundManager()),
    frameCounter(0), currentCacheMemory(0), // Removed text capture members
    currentFps(0.0f), showFps(false) {
 
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
    fontExtraSmall = TTF_OpenFont(FONT_PATH, fontSize / 12);
    if (!fontExtraSmall) {
        std::cerr << "TTF_OpenFont (extra small) failed: " << TTF_GetError() << std::endl;
    }
 
    // Removed text capture initialization
    frameCounter = 0; // Initialize frame counter
}
 
Display::~Display() {
    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (fontExtraSmall) TTF_CloseFont(fontExtraSmall);
    if (screenSurface) SDL_FreeSurface(screenSurface);
    if (backgroundManager) delete backgroundManager;
 
    // Removed text capture cleanup
 
    // Cleanup texture cache
    for (auto& pair : textureCache) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
}
 
// Implement the hash function for SimpleTextKey
std::size_t Display::SimpleTextKeyHash::operator()(const SimpleTextKey& k) const {
    std::size_t h1 = std::hash<std::string>{}(k.text);
    std::size_t h2 = std::hash<void*>{}(k.font);
    // Combine color components into a single integer for hashing
    std::size_t h3 = std::hash<int>{}(k.color.r << 24 | k.color.g << 16 | k.color.b << 8 | k.color.a);
    // Combine hashes using XOR and bit shifts
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}
 
size_t Display::estimateTextureMemory(int width, int height) {
    // Assuming RGBA8888 format (4 bytes per pixel)
    return width * height * 4;
}

void Display::removeOldestTexture() {
    if (textureCache.empty()) return;
 
    auto oldest = textureCache.begin();
    Uint32 oldestFrame = oldest->second.lastUsedFrame;
 
    for (auto it = textureCache.begin(); it != textureCache.end(); ++it) {
        if (it->second.lastUsedFrame < oldestFrame) {
            oldest = it;
            oldestFrame = it->second.lastUsedFrame;
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
                                     SDL_Color color) { // Removed isDynamic parameter
    SimpleTextKey key{text, font, color};
    auto it = textureCache.find(key);
 
    if (it != textureCache.end()) {
        // Update last used frame
        it->second.lastUsedFrame = frameCounter;
        return it->second.texture;
    }

    // Create new texture
    SDL_Rect rect;
    SDL_Texture* texture = createTextTexture(text, font, color, rect);
    if (!texture) return nullptr;

    // Calculate memory size
    size_t memSize = estimateTextureMemory(rect.w, rect.h);
 
    // Manage cache size (only check memory limit now)
    while ((currentCacheMemory + memSize > MAX_CACHE_MEMORY)
           && !textureCache.empty()) {
        removeOldestTexture();
    }
 
    // Add to cache
    currentCacheMemory += memSize;
    textureCache[key] = SimpleCachedTexture{
        texture,
        frameCounter, // Use frameCounter as lastUsedFrame
        rect,
        memSize
    };

    return texture;
}
 
void Display::cleanupCache() {
    std::vector<SimpleTextKey> toRemove;
 
    for (const auto& pair : textureCache) {
        // Simple LRU based on frame count
        if (frameCounter - pair.second.lastUsedFrame > CACHE_LIFETIME) {
            toRemove.push_back(pair.first);
        }
    }

    for (const auto& key : toRemove) {
        currentCacheMemory -= textureCache[key].memorySize;
        SDL_DestroyTexture(textureCache[key].texture);
        textureCache.erase(key);
    }
}
 
void Display::renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY, bool isDynamic /*= false*/) { // isDynamic is unused
    // Get cached or create new texture
    SDL_Texture* texture = getCachedTexture(text, font, color); // Removed isDynamic
    if (!texture) return;
 
    // Get the cached rect for size
    const SDL_Rect& cachedRect = textureCache[SimpleTextKey{text, font, color}].rect; // Use SimpleTextKey
 
    // Calculate position
    SDL_Rect destRect = {
        centerX - cachedRect.w / 2,
        centerY - cachedRect.h / 2,
        cachedRect.w,
        cachedRect.h
    };

    // --- Render Soft Shadow ---
    // Set shadow color modulation and alpha once
    SDL_SetTextureColorMod(texture, SHADOW_COLOR.r, SHADOW_COLOR.g, SHADOW_COLOR.b);
    SDL_SetTextureAlphaMod(texture, SHADOW_COLOR.a); // Use the low shadow alpha

    // Render multiple times in a circle for a soft effect
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        double angle = 2.0 * M_PI * i / SHADOW_SAMPLES;
        int offsetX = static_cast<int>(SHADOW_RADIUS * cos(angle));
        int offsetY = static_cast<int>(SHADOW_RADIUS * sin(angle));

        SDL_Rect shadowDestRect = {
            destRect.x + offsetX,
            destRect.y + offsetY,
            destRect.w,
            destRect.h
        };
 
        // Removed rendering shadow to textCapture
        // Render shadow sample to main screen (uses the currently set target)
        SDL_RenderCopy(renderer, texture, nullptr, &shadowDestRect);
    }
 
    // Reset color and alpha modulation for main text
    SDL_SetTextureColorMod(texture, 255, 255, 255); // Reset color to white
    SDL_SetTextureAlphaMod(texture, color.a);       // Use original text alpha
 
    // --- Render Main Text ---
    // Removed rendering main text to textCapture
    // Render main text to screen (uses the currently set target)
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);
  
    // Increment frame counter
    frameCounter++;
} // Removed isDynamic parameter usage

// Rest of the implementation remains largely unchanged
void Display::renderMultilineText(const std::string& text, TTF_Font* font, SDL_Color color, 
                                int centerX, int startY, float lineSpacing) {
    int maxWidth = static_cast<int>(sizeW * 0.9);
    
    if (!needsTwoLines(text, font, maxWidth)) {
        renderText(text, font, color, centerX, startY);
        return;
    }

    auto lines = splitIntoTwoLines(text);
    int lineHeight = TTF_FontLineSkip(font); // Removed the lineSpacing multiplier

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
    if (!showFps) return;  // Skip rendering if FPS counter is hidden
    
    std::string fpsText = std::to_string(static_cast<int>(currentFps)) + " FPS";
    SDL_Color white = {255, 255, 255, 255};
    
    // Position in top-right corner with 10px padding
    int textWidth, textHeight;
    TTF_SizeText(fontSmall, fpsText.c_str(), &textWidth, &textHeight);
    int x = sizeW - textWidth;
    int y = 1;
    
    renderText(fpsText, fontExtraSmall, white, x + textWidth/2, y + textHeight/2, true);
}

void Display::update() {
    updateFpsCounter();
    renderFpsCounter();
    cleanupCache(); // Clean up old textures from the cache
    // SDL_Renderer is updated in the main loop (Clock::draw) after drawing everything
}
 
// Removed: beginTextCapture()
// Removed: endTextCapture()
// Removed: updateTextCapture()
// Removed: checkTextureChange()
// Removed: isPixelOccupied()
