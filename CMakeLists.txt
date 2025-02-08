cmake_minimum_required(VERSION 3.15)
project(digital_clock_cpp)

add_executable(digital_clock main.cpp clock.cpp display.cpp snow_system.cpp weather_api.cpp weather.cpp clothing_advice.cpp background_manager.cpp)

# Find and link libraries - vcpkg integration handles this
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(libcurl REQUIRED)
find_package(nlohmann_json REQUIRED) # Might be found automatically as header-only
find_package(cpp-httplib REQUIRED) # Might be found automatically as header-only

target_link_libraries(digital_clock PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    libcurl::libcurl
    nlohmann_json::nlohmann_json # If needed, usually header-only is fine
    cpp-httplib::cpp-httplib # If needed, usually header-only is fine
)