#include "display.h"
#include "logger.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// Correct font path
static const char* FONT_PATH = "assets/fonts/BellotaText-Bold.ttf";

// Helper to create font unique_ptr with custom deleter
static std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> makeFontPtr(TTF_Font* font) {
    return std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)>(font, TTF_CloseFont);
}

Display::Display(SDL_Renderer* renderer, int screenWidth, int screenHeight)
    : renderer(renderer)
    , screenWidth(screenWidth)
    , screenHeight(screenHeight)
    , fontLarge(nullptr, TTF_CloseFont)
    , fontSmall(nullptr, TTF_CloseFont)
    , fontExtraSmall(nullptr, TTF_CloseFont)
    , currentCacheMemory(0)
    , currentFps(0.0f)
    , showFps(false)
    , lastFrameTime(std::chrono::steady_clock::now())
    , frameCounter(0)
    , lastCacheCleanup(std::chrono::steady_clock::now())
{
    // Calculate and load fonts
    int largeFontSize = calculateLargeFontSize();

    fontLarge = makeFontPtr(loadFont(FONT_PATH, largeFontSize));
    fontSmall = makeFontPtr(loadFont(FONT_PATH, std::max(1, largeFontSize / 8)));
    fontExtraSmall = makeFontPtr(loadFont(FONT_PATH, std::max(1, largeFontSize / 12)));

    if (!fontLarge || !fontSmall || !fontExtraSmall) {
        LOG_WARNING("Failed to load one or more fonts");
    }
}

Display::~Display() {
    // Smart pointers and unique_ptr in cache handle cleanup automatically
}

TTF_Font* Display::loadFont(const char* path, int size) {
    // Ensure size is at least 1 to prevent TTF_OpenFont failure
    size = std::max(1, size);
    
    TTF_Font* font = TTF_OpenFont(path, size);
    if (!font) {
        LOG_ERROR("Failed to load font %s size %d: %s", path, size, TTF_GetError());
    }
    return font;
}

int Display::calculateLargeFontSize() {
    const char* testText = "22:22";
    int targetHeight = static_cast<int>(screenHeight * 0.8);
    int maxWidth = static_cast<int>(screenWidth * 0.9);

    LOG_DEBUG("Screen size: %dx%d", screenWidth, screenHeight);
    LOG_DEBUG("Target height: %d, max width: %d", targetHeight, maxWidth);

    // Use the original linear search approach
    int testSize = 100;
    int lastGoodSize = 10;

    while (testSize < 2000) {
        TTF_Font* testFont = TTF_OpenFont(FONT_PATH, testSize);

        if (!testFont) {
            LOG_ERROR("Failed to load font %s at size %d: %s", FONT_PATH, testSize, TTF_GetError());
            break;
        }

        int textWidth = 0, textHeight = 0;
        TTF_SizeText(testFont, testText, &textWidth, &textHeight);
        TTF_CloseFont(testFont);

        if (textHeight >= targetHeight || textWidth >= maxWidth) {
            int finalSize = std::max(10, testSize - 10);
            LOG_DEBUG("Final large font size: %d", finalSize);
            return finalSize;
        }

        lastGoodSize = testSize;
        testSize += 10;
    }

    LOG_DEBUG("Final large font size (fallback): %d", lastGoodSize);
    return lastGoodSize;
}

TTF_Font* Display::getFont(FontSize size) const {
    switch (size) {
        case FontSize::LARGE: return fontLarge.get();
        case FontSize::SMALL: return fontSmall.get();
        case FontSize::EXTRA_SMALL: return fontExtraSmall.get();
        default: return fontSmall.get();
    }
}

std::size_t Display::CacheKeyHash::operator()(const CacheKey& k) const {
    // Better hash combining using the standard pattern
    std::size_t seed = 0;
    
    // Hash text
    seed ^= std::hash<std::string>{}(k.text) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    // Hash font size
    seed ^= std::hash<int>{}(static_cast<int>(k.fontSize)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    // Hash color as a single value
    Uint32 colorValue = (k.color.r << 24) | (k.color.g << 16) | (k.color.b << 8) | k.color.a;
    seed ^= std::hash<Uint32>{}(colorValue) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    return seed;
}

SDL_Texture* Display::createTextTexture(const std::string& text, TTF_Font* font, 
                                        SDL_Color color, int& outWidth, int& outHeight) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        LOG_ERROR("TTF_RenderUTF8_Blended failed: %s", TTF_GetError());
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        LOG_ERROR("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return nullptr;
    }

    outWidth = surface->w;
    outHeight = surface->h;
    SDL_FreeSurface(surface);
    
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

SDL_Texture* Display::getOrCreateTexture(const std::string& text, FontSize size, SDL_Color color,
                                        int* outWidth, int* outHeight) {
    CacheKey key{text, size, color};
    
    // Check cache
    auto it = textureCache.find(key);
    if (it != textureCache.end()) {
        it->second.lastUsed = std::chrono::steady_clock::now();
        if (outWidth) *outWidth = it->second.width;
        if (outHeight) *outHeight = it->second.height;
        return it->second.texture.get();
    }

    // Create new texture
    TTF_Font* font = getFont(size);
    if (!font) return nullptr;

    int width, height;
    SDL_Texture* rawTexture = createTextTexture(text, font, color, width, height);
    if (!rawTexture) return nullptr;

    // Estimate memory usage (RGBA8888 = 4 bytes per pixel)
    size_t memorySize = width * height * 4;

    // Evict old entries if needed
    while (currentCacheMemory + memorySize > MAX_CACHE_MEMORY && !textureCache.empty()) {
        removeOldestCacheEntry();
    }

    // Add to cache
    currentCacheMemory += memorySize;
    auto result = textureCache.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(key),
        std::forward_as_tuple(rawTexture, width, height, memorySize)
    );

    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;
    return result.first->second.texture.get();
}

void Display::removeOldestCacheEntry() {
    if (textureCache.empty()) return;

    auto oldest = textureCache.begin();
    for (auto it = textureCache.begin(); it != textureCache.end(); ++it) {
        if (it->second.lastUsed < oldest->second.lastUsed) {
            oldest = it;
        }
    }

    currentCacheMemory -= oldest->second.memorySize;
    textureCache.erase(oldest);
}

void Display::renderTextureWithShadow(SDL_Texture* texture, const SDL_Rect& rect, 
                                      SDL_Color color, bool withShadow) {
    if (withShadow) {
        // Render shadow
        SDL_Rect shadowRect = {
            rect.x + SHADOW_OFFSET,
            rect.y + SHADOW_OFFSET,
            rect.w,
            rect.h
        };
        
        SDL_SetTextureColorMod(texture, 0, 0, 0);
        SDL_SetTextureAlphaMod(texture, SHADOW_ALPHA);
        SDL_RenderCopy(renderer, texture, nullptr, &shadowRect);
    }

    // Render main text
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
}

void Display::renderText(const std::string& text, FontSize size, const TextStyle& style, 
                        int x, int y) {
    int width, height;
    SDL_Texture* texture = getOrCreateTexture(text, size, style.color, &width, &height);
    if (!texture) return;

    // Calculate position based on alignment
    int posX = x;
    switch (style.alignment) {
        case TextAlign::CENTER:
            posX = x - width / 2;
            break;
        case TextAlign::RIGHT:
            posX = x - width;
            break;
        case TextAlign::LEFT:
        default:
            break;
    }

    // Always center vertically (like the original code)
    int posY = y - height / 2;

    SDL_Rect destRect = {posX, posY, width, height};
    renderTextureWithShadow(texture, destRect, style.color, style.withShadow);
}

std::vector<std::string> Display::wrapText(const std::string& text, TTF_Font* font, int maxWidth) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string word, currentLine;

    while (stream >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        
        int textWidth = 0;
        TTF_SizeUTF8(font, testLine.c_str(), &textWidth, nullptr);
        
        if (textWidth <= maxWidth) {
            currentLine = testLine;
        } else {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
            }
            currentLine = word;
            
            // Handle words longer than max width
            TTF_SizeUTF8(font, word.c_str(), &textWidth, nullptr);
            if (textWidth > maxWidth) {
                lines.push_back(word);
                currentLine.clear();
            }
        }
    }
    
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }
    
    return lines;
}

void Display::renderMultilineText(const std::string& text, FontSize size, const TextStyle& style,
                                  int x, int y, int maxWidth) {
    TTF_Font* font = getFont(size);
    if (!font) return;

    // Use 90% of screen width if no max width specified
    if (maxWidth == 0) {
        maxWidth = static_cast<int>(screenWidth * 0.9);
    }

    std::vector<std::string> lines = wrapText(text, font, maxWidth);
    int lineHeight = TTF_FontLineSkip(font);

    for (size_t i = 0; i < lines.size(); ++i) {
        int lineY = y + static_cast<int>(i) * lineHeight;
        renderText(lines[i], size, style, x, lineY);
    }
}

void Display::updateFps() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameTime);
    
    if (duration.count() > 0) {
        currentFps = 1000000.0f / duration.count();
    }
    
    lastFrameTime = now;
}

void Display::renderFps() {
    if (!showFps) return;

    std::string fpsText = std::to_string(static_cast<int>(currentFps)) + " FPS";
    
    TextStyle style;
    style.color = {255, 255, 255, 255};
    style.alignment = TextAlign::RIGHT;
    style.withShadow = true;
    
    renderText(fpsText, FontSize::EXTRA_SMALL, style, screenWidth - 10, 10);
}

void Display::cleanupCache() {
    auto now = std::chrono::steady_clock::now();
    
    // Only cleanup every CACHE_CLEANUP_INTERVAL_SECONDS seconds
    auto timeSinceLastCleanup = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastCacheCleanup
    );
    
    if (timeSinceLastCleanup.count() < CACHE_CLEANUP_INTERVAL_SECONDS) {
        return;  // Skip cleanup if not enough time has passed
    }
    
    lastCacheCleanup = now;
    
    // Use iterator directly for efficient removal
    for (auto it = textureCache.begin(); it != textureCache.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastUsed
        );
        
        if (age.count() > CACHE_LIFETIME_SECONDS) {
            currentCacheMemory -= it->second.memorySize;
            it = textureCache.erase(it);  // Erase returns next valid iterator
        } else {
            ++it;  // Only increment if we didn't erase
        }
    }
}

void Display::clearCache() {
    textureCache.clear();
    currentCacheMemory = 0;
}