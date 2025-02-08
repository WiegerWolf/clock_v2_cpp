cmake_minimum_required(VERSION 3.15) # Or higher, check your Conan requirements

project(digital_clock)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(httplib REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    clock.cpp
    display.cpp
    snow_system.cpp
    weather_api.cpp
    weather.cpp
    clothing_advice.cpp
    background_manager.cpp
    constants.cpp
)

target_link_libraries(${PROJECT_NAME} 
    SDL2::SDL2 
    SDL2_image::SDL2_image 
    SDL2_ttf::SDL2_ttf-static 
    nlohmann_json::nlohmann_json 
    httplib::httplib 
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads)

target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)