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

// Hash structure for text rendering parameters
struct TextKey {
    std::string text;
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
    }
};

// Custom hash function for TextKey
struct TextKeyHash {
    std::size_t operator()(const TextKey& k) const {
        std::size_t h1 = std::hash<std::string>{}(k.text);
        std::size_t h2 = std::hash<void*>{}(k.font);
        std::size_t h3 = std::hash<int>{}(k.color.r << 24 | k.color.g << 16 | k.color.b << 8 | k.color.a);
        std::size_t h4 = std::hash<bool>{}(k.isDynamic);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

// Cached texture with metadata
struct CachedTexture {
    SDL_Texture* texture;
    Uint32 lastUsed;     // Frame counter when last used
    SDL_Rect rect;       // Size only for static textures
    size_t memorySize;   // Estimated memory usage in bytes
};

class Display {
public:
    Display(SDL_Renderer* renderer, int width, int height);
    ~Display();

    void clear();
    void update();
    void renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY, bool isDynamic = false);
    void renderMultilineText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int startY, float lineSpacing);

    bool isPixelOccupied(int x, int y) const;
    void beginTextCapture();
    void endTextCapture();

    bool hasTextureChanged() const { return textureChanged; }
    void resetTextureChangeFlag() { textureChanged = false; }

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
    static const Uint32 CACHE_LIFETIME = 300;        // ~5 seconds at 60fps
    static const size_t MAX_CACHE_SIZE = 100;        // Maximum number of cached textures
    static const size_t MAX_CACHE_MEMORY = 32*1024*1024;  // 32MB max texture cache
    size_t currentCacheMemory;                       // Current texture cache memory usage

    int calculateFontSize();
    std::vector<std::string> wrapText(const std::string& text, TTF_Font* font, int maxWidth);
    bool needsTwoLines(const std::string& text, TTF_Font* font, int maxWidth);
    std::pair<std::string, std::string> splitIntoTwoLines(const std::string& text);

    SDL_Texture* textCapture;
    void updateTextCapture();
    Uint32* textPixels;
    int texturePitch;
    SDL_Texture* mainTarget;

    bool textureChanged;
    Uint32* previousTextPixels;
    void checkTextureChange();

    // Texture caching
    std::unordered_map<TextKey, CachedTexture, TextKeyHash> textureCache;
    SDL_Texture* getCachedTexture(const std::string& text, TTF_Font* font, 
                               SDL_Color color, bool isDynamic = false);
    void cleanupCache();
    SDL_Texture* createTextTexture(const std::string& text, TTF_Font* font, SDL_Color color, SDL_Rect& outRect);
    
    // Memory management
    size_t estimateTextureMemory(int width, int height);
    void removeOldestTexture();
};

#endif // DISPLAY_H