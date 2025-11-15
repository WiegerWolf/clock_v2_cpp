// clothing_advice.cpp
#include "clothing_advice.h"
#include "config.h"
#include "constants.h" // Include constants for API details
#include "weather.h"
#include "logger.h"
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
// API Key is now read directly from constants.h/cpp via CEREBRAS_API_KEY
if (!CEREBRAS_API_KEY || std::string(CEREBRAS_API_KEY).empty()) {
    LOG_WARNING("Cerebras API Key is not configured. Falling back to basic advice.");
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
        {"model", CEREBRAS_MODEL},
        {"max_tokens", 300}, // Adjusted max_tokens, 312 might be too long for just clothing advice
        {"temperature", 0.7}, // Slightly increased temperature for potentially more varied advice
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

try {
    // Use SSLClient for HTTPS connection to Cerebras
    httplib::SSLClient cli(CEREBRAS_API_HOST, CEREBRAS_API_PORT);
    cli.set_connection_timeout(10); // 10 seconds
    cli.set_read_timeout(10);

    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"Authorization", std::string("Bearer ") + CEREBRAS_API_KEY},
    };

    // Post to the Cerebras API path
    auto res = cli.Post(CEREBRAS_API_PATH, headers, payload.dump(), "application/json");

    if (res) {
        if (res->status == 200) {
            try {
                nlohmann::json j = nlohmann::json::parse(res->body);
                // Check for Cerebras specific error structure if needed, or general structure
                if (j.contains("error")) {
                    std::string errorMsg = j["error"].dump();
                    LOG_ERROR("Cerebras API Error: %s", errorMsg.c_str());
                    return getBasicAdvice(temperature);
                }
                // Add proper validation before accessing nested JSON fields
                if (j.contains("choices") && !j["choices"].empty()) {
                    auto& choice = j["choices"][0];
                    if (choice.contains("message") && choice["message"].contains("content")) {
                        auto& content = choice["message"]["content"];
                        // Check if content is not null and is a string
                        if (!content.is_null() && content.is_string()) {
                            std::string advice = content.get<std::string>();
                            return !advice.empty() ? advice : getBasicAdvice(temperature);
                        }
                    }
                }
                LOG_ERROR("Invalid or empty response from Cerebras API");
                return getBasicAdvice(temperature);
            } catch (const nlohmann::json::parse_error &e) {
                LOG_ERROR("Error processing Cerebras JSON response: %s", e.what());
                return getBasicAdvice(temperature);
            } catch (const std::exception &e) {
                LOG_ERROR("Error extracting content from response: %s", e.what());
                return getBasicAdvice(temperature);
            }
        }
    } else {
        auto err = res.error();
        LOG_ERROR("HTTP request failed: %s", httplib::to_string(err).c_str());
        return getBasicAdvice(temperature);
    }
} catch (const std::exception &e) {
    LOG_ERROR("An unexpected error occurred: %s", e.what());
    return getBasicAdvice(temperature);
}

    // Fallback to basic advice if API call fails or returns empty/invalid data
    return getBasicAdvice(temperature);
}
