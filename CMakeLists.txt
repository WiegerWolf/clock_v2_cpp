cmake_minimum_required(VERSION 3.15)
include(FetchContent)

project(digital_clock)

# Get Git commit hash (short 7-character version)
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback if not in Git repo
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

# Get build timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

# Generate version header
configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/version.h
    @ONLY
)

# Check if we're building for Raspberry Pi
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    # Raspberry Pi specific optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math")
else()
    # Generic optimization flags for other platforms
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math")
endif()

# Enable Link Time Optimization
# Disabled due to GCC 12 ICE with cpp-httplib template instantiation
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Workaround for GCC 12 ARM ICE with cpp-httplib regex instantiation
# Compile weather_api.cpp with reduced optimization to avoid the compiler bug
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set_source_files_properties(weather_api.cpp PROPERTIES 
        COMPILE_FLAGS "-O2 -fno-tree-vrp")
endif()

# Option to provide Cerebras API Key at compile time
option(CEREBRAS_API_KEY_DEFINE "Provide the Cerebras API Key" "")
if(NOT CEREBRAS_API_KEY_DEFINE)
    message(FATAL_ERROR "CEREBRAS_API_KEY_DEFINE must be provided via CMake options, e.g., -DCEREBRAS_API_KEY_DEFINE='your-key'")
endif()

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
    config.cpp
    logger.cpp
    http_client.cpp
)

# Include build directory for generated headers
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

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

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    CEREBRAS_API_KEY_DEFINE="${CEREBRAS_API_KEY_DEFINE}"
)
