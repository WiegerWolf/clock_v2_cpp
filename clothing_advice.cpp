// clothing_advice.cpp
#include "clothing_advice.h"
#include "config.h"
#include "constants.h" // Include constants for API details
#include "weather.h"
#include <string>
#include <sstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <ctime>
#include <iomanip>
#include <iostream> // For std::cerr

using json = nlohmann::json;

std::string getBasicAdvice(double temperature) {
    if (temperature < -10) {
        return "Наденьте теплую зимнюю куртку, шапку, шарф и теплые ботинки";
    } else if (temperature < 0) {
        return "Наденьте зимнюю куртку и теплые аксессуары";
    } else if (temperature < 10) {
        return "Наденьте куртку и шапку";
    } else if (temperature < 20) {
        return "Наденьте легкую куртку или свитер";
    } else {
        return "Наденьте легкую одежду";
    }
}

// Updated function signature to match header (apiKey removed)
std::string getClothingAdvice(double temperature, int weathercode, double windspeed, const char* language) {
    // API Key is now read directly from constants.h/cpp via OPENROUTER_API_KEY
    if (!OPENROUTER_API_KEY || std::string(OPENROUTER_API_KEY).empty()) {
        std::cerr << "OpenRouter API Key is not configured. Falling back to basic advice." << std::endl;
        return getBasicAdvice(temperature);
    }

    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    std::stringstream monthStream;
    monthStream << std::put_time(now, "%B");
    std::string currentMonth = monthStream.str();
    std::stringstream timeStream;
    timeStream << std::put_time(now, "%H:%M");
    std::string currentTime = timeStream.str();

    // Pass true to getWeatherDescription to include windspeed details for the LLM
    std::string weatherDesc = getWeatherDescription(temperature, weathercode, windspeed, true);

    json payload = {
        {"model", OPENROUTER_MODEL},
        {"max_tokens", 150}, // Adjusted max_tokens, 312 might be too long for just clothing advice
        {"temperature", 0.5}, // Slightly increased temperature for potentially more varied advice
        {"messages", {
            {{"role", "system"}, {"content", "You are a helpful assistant providing concise clothing advice."}},
            {{"role", "user"}, {"content",
                "I live in Amsterdam.\n" // Keep location context if useful for the model
                "Today is " + std::to_string(now->tm_mday) + " " + currentMonth + ", \n"
                "the time is " + currentTime + ", \n"
                "and the weather is: " + weatherDesc + ". \n"
                "What should I wear? \n"
                "Please answer in one short sentence, using this locale: " + std::string(language) + ".\n"
                "Only say what clothes I should wear, there's no need to mention city, current weather or time and date.\n"
                "Basically, just continue the phrase: You should wear..., without saying the 'you should wear' part.\n"
            }}
        }}
    };

    // Use SSLClient for HTTPS connection to OpenRouter
    httplib::SSLClient cli(OPENROUTER_API_HOST, OPENROUTER_API_PORT);
    cli.set_connection_timeout(5); // 5 seconds connection timeout
    cli.set_read_timeout(10);      // Increased read timeout for potentially slower API responses
    cli.set_write_timeout(5);      // 5 seconds write timeout
    cli.enable_server_certificate_verification(false); // Consider enabling verification in production

    httplib::Headers headers = {
        {"Authorization", std::string("Bearer ") + OPENROUTER_API_KEY},
        {"Content-Type", "application/json"},
        {"HTTP-Referer", OPENROUTER_REFERER}, // Optional OpenRouter header
        {"X-Title", OPENROUTER_TITLE}         // Optional OpenRouter header
    };

    // Post to the OpenRouter API path
    auto res = cli.Post(OPENROUTER_API_PATH, headers, payload.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json data = json::parse(res->body);
            // Check for OpenRouter specific error structure if needed, or general structure
            if (data.contains("error")) {
                 // Attempt to extract message, handle potential variations in error structure
                std::string errorMsg = "Unknown API error";
                if (data["error"].is_object() && data["error"].contains("message")) {
                    errorMsg = data["error"]["message"].get<std::string>();
                } else if (data["error"].is_string()) {
                    errorMsg = data["error"].get<std::string>();
                }
                std::cerr << "OpenRouter API Error: " << errorMsg << std::endl;
                return getBasicAdvice(temperature);
            }
            if (data.contains("choices") && !data["choices"].empty()) {
                std::string advice = data["choices"][0]["message"]["content"].get<std::string>();
                return !advice.empty() ? advice : getBasicAdvice(temperature);
            }
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error processing OpenRouter JSON response: " << e.what() << std::endl;
        }
    } else {
        std::string errorBody = res ? res->body : "No response body";
        std::cerr << "OpenRouter API request failed: "
                  << (res ? std::to_string(res->status) : "No response")
                  << " - " << errorBody << std::endl;
    }

    // Fallback to basic advice if API call fails or returns empty/invalid data
    return getBasicAdvice(temperature);
}
