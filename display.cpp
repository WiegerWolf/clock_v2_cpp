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
    textureChanged(false), previousTextPixels(nullptr) {
    screenSurface = SDL_CreateRGBSurface(0, sizeW, sizeH, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!screenSurface) {
        std::cerr << "SDL_CreateRGBSurface failed: " << SDL_GetError() << std::endl;
        // Handle error, maybe throw exception or return false
    }

    int fontSize = calculateFontSize();
    fontLarge = TTF_OpenFont(FONT_PATH, fontSize);
    if (!fontLarge) {
        std::cerr << "TTF_OpenFont (large) failed: " << TTF_GetError() << std::endl;
        // Handle error
    }
    fontSmall = TTF_OpenFont(FONT_PATH, fontSize / 8);
    if (!fontSmall) {
        std::cerr << "TTF_OpenFont (small) failed: " << TTF_GetError() << std::endl;
        // Handle error
    }

    // Create target textures
    mainTarget = SDL_GetRenderTarget(renderer);
    textCapture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET, width, height);
    if (!textCapture) {
        std::cerr << "Failed to create text capture texture: " << SDL_GetError() << std::endl;
    }
    texturePitch = width * 4; // 4 bytes per pixel (RGBA)
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

void Display::renderText(const std::string& text, TTF_Font* font, SDL_Color color, int centerX, int centerY) {
    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!textSurface) {
        std::cerr << "TTF_RenderUTF8_Blended failed: " << TTF_GetError() << std::endl;
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(textSurface);
        return;
    }

    SDL_Rect textRect;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;
    textRect.x = centerX - textRect.w / 2;
    textRect.y = centerY - textRect.h / 2;

    // Render to both textures
    SDL_Texture* currentTarget = SDL_GetRenderTarget(renderer);
    
    // Render to collision texture
    SDL_SetRenderTarget(renderer, textCapture);
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    
    // Render to screen
    SDL_SetRenderTarget(renderer, mainTarget);
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    
    // Restore original target
    SDL_SetRenderTarget(renderer, currentTarget);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
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
    // Store current render target and switch to text capture
    mainTarget = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, textCapture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
}

void Display::endTextCapture() {
    // Capture the current text state
    updateTextCapture();
    // Switch back to main render target
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
    return alpha > 50; // Consider pixels with alpha > 50 as solid
}