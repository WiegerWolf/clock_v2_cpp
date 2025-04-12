// display.h
#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
 
class BackgroundManager;
 
// Removed original TextKey, TextKeyHash, CachedTexture structs
 
class Display {
public:
    TTF_Font* font;
    SDL_Color color;
    bool isDynamic;  // Whether this is dynamic content (time, etc.)

    bool operator==(const TextKey& other) const {
        return text == other.text &&
               font == other.font &&
               color.r == other.color.r &&
               color.g == other.color.g &&
               color.b == other.color.b &&
               color.a == other.color.a &&
               isDynamic == other.isDynamic;
    Display(SDL_Renderer* renderer, int width, int height);
    ~Display();

    void clear();
    void update();
    void renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY, bool isDynamic = false); // isDynamic is now unused internally
    void renderMultilineText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int startY, float lineSpacing = 1.0f); // Default line spacing
 
    // Removed text capture related methods:
    // isPixelOccupied, beginTextCapture, endTextCapture, hasTextureChanged, resetTextureChangeFlag

    void setFpsVisible(bool visible) { showFps = visible; }
    bool isFpsVisible() const { return showFps; }

    TTF_Font* fontLarge;
    TTF_Font* fontSmall;
    TTF_Font* fontExtraSmall;
    SDL_Renderer* renderer;
    SDL_Surface* screenSurface;
    BackgroundManager* backgroundManager;

private:
    int sizeW, sizeH;
    Uint32 frameCounter;  // Track frames for cache management
 
    // FPS counter
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    float currentFps;
    void updateFpsCounter();
    void renderFpsCounter();
 
    // Texture caching (simplified - no dynamic flag, frame counter, memory tracking needed anymore)
    struct SimpleTextKey {
        std::string text;
        TTF_Font* font;
        SDL_Color color;
 
        bool operator==(const SimpleTextKey& other) const {
            return text == other.text && font == other.font &&
                   color.r == other.color.r && color.g == other.color.g &&
                   color.b == other.color.b && color.a == other.color.a;
        }
    };
    struct SimpleTextKeyHash {
        std::size_t operator()(const SimpleTextKey& k) const; // Implementation in .cpp
    };
    static const Uint32 CACHE_LIFETIME = 300;        // ~5 seconds at 60fps
    // static const size_t MAX_CACHE_SIZE = 100;     // Removed - only memory limit matters now
    static const size_t MAX_CACHE_MEMORY = 32*1024*1024;  // 32MB max texture cache
    size_t currentCacheMemory;                       // Current texture cache memory usage
 
    int calculateFontSize();
    std::vector<std::string> wrapText(const std::string& text, TTF_Font* font, int maxWidth);
    bool needsTwoLines(const std::string& text, TTF_Font* font, int maxWidth);
    std::pair<std::string, std::string> splitIntoTwoLines(const std::string& text);
 
    // Removed text capture members:
    // textCapture, textPixels, texturePitch, mainTarget, textureChanged, previousTextPixels
    // Removed text capture methods:
    // updateTextCapture, checkTextureChange
 
    // Texture caching
    struct SimpleCachedTexture {
        SDL_Texture* texture;
        Uint32 lastUsedFrame; // Use frame counter for simple LRU
        SDL_Rect rect;        // Store size
        size_t memorySize;    // Estimated memory
    };
    std::unordered_map<SimpleTextKey, SimpleCachedTexture, SimpleTextKeyHash> textureCache;
    SDL_Texture* getCachedTexture(const std::string& text, TTF_Font* font, SDL_Color color); // Removed isDynamic
    void cleanupCache();
    SDL_Texture* createTextTexture(const std::string& text, TTF_Font* font, SDL_Color color, SDL_Rect& outRect);
 
    // Memory management
    size_t estimateTextureMemory(int width, int height);
    void removeOldestTexture();

    bool showFps = false;  // FPS counter visibility flag
};

#endif // DISPLAY_H
