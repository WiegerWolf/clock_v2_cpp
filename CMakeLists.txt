cmake_minimum_required(VERSION 3.15) # Or your minimum required CMake version
project(digital_clock_cpp)

# Include Conan toolchain - this is crucial for Conan integration
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(digital_clock main.cpp clock.cpp display.cpp snow_system.cpp weather_api.cpp weather.cpp clothing_advice.cpp background_manager.cpp)

# Link the libraries - Conan's CMakeDeps generator makes this easy
target_link_libraries(digital_clock PRIVATE
    ${CONAN_LIBS} # This variable contains all libraries from conanfile.txt
)