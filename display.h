#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <vector>  // <-- ADD THIS

// Text alignment options
enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

// Text rendering options
struct TextStyle {
    SDL_Color color = {255, 255, 255, 255};
    bool withShadow = true;
    TextAlign alignment = TextAlign::CENTER;
};

// Font size categories
enum class FontSize {
    EXTRA_SMALL,
    SMALL,
    LARGE
};

class Display {
public:
    Display(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    ~Display();

    // Main rendering methods
    void renderText(const std::string& text, FontSize size, const TextStyle& style, int x, int y);
    void renderMultilineText(const std::string& text, FontSize size, const TextStyle& style, 
                            int x, int y, int maxWidth = 0);

    // FPS counter
    void updateFps();
    void renderFps();
    void setFpsVisible(bool visible) { showFps = visible; }

    // Cache management
    void cleanupCache();
    void clearCache();

    // Get font for external size calculations if needed
    TTF_Font* getFont(FontSize size) const;

private:
    // Core resources
    SDL_Renderer* renderer;
    int screenWidth;
    int screenHeight;

    // Fonts
    std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> fontLarge;
    std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> fontSmall;
    std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> fontExtraSmall;

    // Text cache
    struct CacheKey {
        std::string text;
        FontSize fontSize;
        SDL_Color color;

        bool operator==(const CacheKey& other) const {
            return text == other.text && 
                   fontSize == other.fontSize &&
                   color.r == other.color.r &&
                   color.g == other.color.g &&
                   color.b == other.color.b &&
                   color.a == other.color.a;
        }
    };

    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& k) const;
    };

    struct CachedTexture {
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture;
        int width;
        int height;
        std::chrono::steady_clock::time_point lastUsed;
        size_t memorySize;

        CachedTexture(SDL_Texture* tex, int w, int h, size_t mem)
            : texture(tex, SDL_DestroyTexture), width(w), height(h), 
              lastUsed(std::chrono::steady_clock::now()), memorySize(mem) {}
    };

    std::unordered_map<CacheKey, CachedTexture, CacheKeyHash> textureCache;
    size_t currentCacheMemory;

    // FPS tracking
    float currentFps;
    bool showFps;
    std::chrono::steady_clock::time_point lastFrameTime;
    size_t frameCounter;  // Track frames for periodic cache cleanup
    std::chrono::steady_clock::time_point lastCacheCleanup;

    // Configuration
    static constexpr size_t MAX_CACHE_MEMORY = 50 * 1024 * 1024; // 50MB
    static constexpr int CACHE_LIFETIME_SECONDS = 30;
    static constexpr int CACHE_CLEANUP_INTERVAL_SECONDS = 5;  // Cleanup every 5 seconds
    static constexpr int SHADOW_OFFSET = 2;
    static constexpr Uint8 SHADOW_ALPHA = 128;

    // Helper methods
    TTF_Font* loadFont(const char* path, int size);
    int calculateLargeFontSize();
    
    SDL_Texture* getOrCreateTexture(const std::string& text, FontSize size, SDL_Color color);
    SDL_Texture* createTextTexture(const std::string& text, TTF_Font* font, SDL_Color color, 
                                   int& outWidth, int& outHeight);
    
    void renderTextureWithShadow(SDL_Texture* texture, const SDL_Rect& rect, 
                                 SDL_Color color, bool withShadow);
    
    std::vector<std::string> wrapText(const std::string& text, TTF_Font* font, int maxWidth);
    void removeOldestCacheEntry();
};

#endif // DISPLAY_H