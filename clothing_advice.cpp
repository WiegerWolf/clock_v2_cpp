// clothing_advice.cpp
#include "clothing_advice.h"
#include "config.h"
#include "weather.h"
#include <string>
#include <sstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <ctime>
#include <iomanip>

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

std::string getClothingAdvice(double temperature, int weathercode, double windspeed, const char* apiKey, const char* language) {
    if (!apiKey || std::string(apiKey).empty()) {
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

    std::string weatherDesc = getWeatherDescription(temperature, weathercode, windspeed);

    json payload = {
        {"model", "llama-3.3-70b-versatile"},
        {"max_tokens", 312},
        {"temperature", 0.0},
        {"messages", {
            {{"role", "system"}, {"content", "You are a helpful assistant."}},
            {{"role", "user"}, {"content",
                "I live in Amsterdam. \n"
                "Today is " + std::to_string(now->tm_mday) + " " + currentMonth + ", \n"
                "the time is " + currentTime + ", \n"
                "and the weather is: " + weatherDesc + ". \n"
                "What should I wear? \n"
                "Please answer in one short sentence, using this locale: " + std::string(language) + ".\n"
                "Only say what clothes I should wear, there's no need to mention city, current weather or time and date.\n"
                "Basically, just continue the phrase: You should wear..."
            }}
        }}
    };

    httplib::Client cli("192.168.68.160", 3000);
    cli.set_connection_timeout(5); // 5 seconds connection timeout
    cli.set_read_timeout(5);       // 5 seconds read timeout
    cli.set_write_timeout(5);      // 5 seconds write timeout

    httplib::Headers headers = {
        {"Authorization", std::string("Bearer ") + apiKey},
        {"Content-Type", "application/json"}
    };
    auto res = cli.Post("/v1/chat/completions", headers, payload.dump(), "application/json");

    if (res && res->status == 200) {  // Changed from status() to status
        try {
            json data = json::parse(res->body);
            if (data.contains("error")) {
                std::cerr << "API Error: " << data["error"].value("message", "Unknown error") << std::endl;
                return getBasicAdvice(temperature);
            }
            if (data.contains("choices") && !data["choices"].empty()) {
                std::string advice = data["choices"][0]["message"]["content"].get<std::string>();
                return !advice.empty() ? advice : getBasicAdvice(temperature);
            }
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error processing clothing advice JSON: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "API request failed: " << (res ? std::to_string(res->status) : "No response") << std::endl;  // Changed from status() to status
    }

    return getBasicAdvice(temperature);
}