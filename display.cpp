// display.cpp
#include "display.h"
#include "config.h"
#include "constants.h"
#include "background_manager.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <climits>  // Add this line for INT_MAX

Display::Display(SDL_Renderer* renderTarget, int width, int height)
    : renderer(renderTarget), sizeW(width), sizeH(height), fontLarge(nullptr), fontSmall(nullptr), backgroundManager(new BackgroundManager()) {
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
}

Display::~Display() {
    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (screenSurface) SDL_FreeSurface(screenSurface);
    if (backgroundManager) delete backgroundManager;
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

    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
}

void Display::clear() {
    SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, BLACK_COLOR.r, BLACK_COLOR.g, BLACK_COLOR.b));
    backgroundManager->update(sizeW, sizeH);

    SDL_Texture* screenTexture = SDL_CreateTextureFromSurface(renderer, screenSurface);
    if (screenTexture) {
        SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
        SDL_DestroyTexture(screenTexture);
    }

    backgroundManager->draw(renderer);
}

void Display::update() {
    // SDL_Renderer is updated in the main loop after drawing everything.
}