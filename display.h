// display.h
#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector> // Add this line
#include <string>

class BackgroundManager;

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

    TTF_Font* fontLarge;
    TTF_Font* fontSmall;
    SDL_Renderer* renderer;
    SDL_Surface* screenSurface;
    BackgroundManager* backgroundManager;

private:
    int sizeW, sizeH;
    int calculateFontSize();
    std::vector<std::string> wrapText(const std::string& text, TTF_Font* font, int maxWidth);
    bool needsTwoLines(const std::string& text, TTF_Font* font, int maxWidth);
    std::pair<std::string, std::string> splitIntoTwoLines(const std::string& text);

    SDL_Texture* textCapture;
    void updateTextCapture();
    Uint32* textPixels;
    int texturePitch;
    SDL_Texture* mainTarget;  // Add this line to store the main render target

    bool textureChanged;
    Uint32* previousTextPixels;
    void checkTextureChange();
};

#endif // DISPLAY_H