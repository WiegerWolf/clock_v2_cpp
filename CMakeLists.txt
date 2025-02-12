cmake_minimum_required(VERSION 3.15)
include(FetchContent)

project(digital_clock)

# Check if we're building for Raspberry Pi
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    # Raspberry Pi specific optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math -pg -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math")
else()
    # Generic optimization flags for other platforms
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -pg -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math")
endif()

# Enable Link Time Optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
FetchContent_Declare(json 
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)
FetchContent_Declare(cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.7
)
FetchContent_MakeAvailable(cpp-httplib)
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