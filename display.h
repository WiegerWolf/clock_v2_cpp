// display.h
#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <unordered_map>

class BackgroundManager;

struct TextureCache {
    SDL_Texture* texture;
    SDL_Rect rect;
    std::string text;
    int fontSize;
    Uint32 lastUsed;
};

class Display {
public:
    Display(SDL_Renderer* renderer, int width, int height);
    ~Display();

    void clear();
    void update();
    void renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY);
    void renderMultilineText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int startY, float lineSpacing);

    bool isPixelOccupied(int x, int y) const;
    void beginTextCapture();
    void endTextCapture();

    bool hasTextureChanged() const { return textureChanged; }
    void resetTextureChangeFlag() { textureChanged = false; }
    void cleanupOldCachedTextures(Uint32 currentTime);

    TTF_Font* fontLarge;
    TTF_Font* fontSmall;
    SDL_Renderer* renderer;
    SDL_Surface* screenSurface;
    BackgroundManager* backgroundManager;

private:
    static const Uint32 TEXTURE_CACHE_LIFETIME = 5000; // 5 seconds
    static const size_t MAX_CACHE_SIZE = 50;

    int sizeW, sizeH;
    int calculateFontSize();
    std::vector<std::string> wrapText(const std::string& text, TTF_Font* font, int maxWidth);
    bool needsTwoLines(const std::string& text, TTF_Font* font, int maxWidth);
    std::pair<std::string, std::string> splitIntoTwoLines(const std::string& text);

    SDL_Texture* textCapture;
    void updateTextCapture();
    Uint32* textPixels;
    int texturePitch;
    SDL_Texture* mainTarget;
    SDL_Texture* batchTexture;  // Move batchTexture to class member

    bool textureChanged;
    Uint32* previousTextPixels;
    void checkTextureChange();

    std::unordered_map<std::string, TextureCache> textureCache;
    SDL_Texture* getCachedTexture(const std::string& text, TTF_Font* font, SDL_Color color, SDL_Rect& outRect);
};

#endif // DISPLAY_H