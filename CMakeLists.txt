cmake_minimum_required(VERSION 3.15) # Or higher, check your Conan requirements

project(digital_clock)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)


add_executable(${PROJECT_NAME}
    main.cpp
    clock.cpp
    display.cpp
    snow_system.cpp
    weather_api.cpp
    weather.cpp
    clothing_advice.cpp
    background_manager.cpp
)

target_link_libraries(${PROJECT_NAME} SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf-static)