from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
import os

class MyProject(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    def requirements(self):
        self.requires("sdl/2.28.3")
        self.requires("sdl_image/2.8.2")
        self.requires("sdl_ttf/2.24.0")
        self.requires("libcurl/8.11.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("cpp-httplib/0.18.3")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        if self.settings.arch == "armv7hf":
            tc.variables["CMAKE_C_COMPILER"] = "arm-linux-gnueabihf-gcc-13" # Explicit compiler paths
            tc.variables["CMAKE_CXX_COMPILER"] = "arm-linux-gnueabihf-g++-13"
            tc.variables["CMAKE_FIND_ROOT_PATH"] = "/usr/arm-linux-gnueabihf;/usr/lib/arm-linux-gnueabihf;/usr/include/arm-linux-gnueabihf" # More explicit paths
            tc.variables["CMAKE_SYSROOT"] = "/usr/arm-linux-gnueabihf"
            tc.variables["CMAKE_C_FLAGS"] = "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
            tc.variables["CMAKE_CXX_FLAGS"] = "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"

            # Force CMake to search only in the sysroot for libraries and includes
            tc.variables["CMAKE_FIND_ROOT_PATH_MODE_PROGRAM"] = "NEVER"
            tc.variables["CMAKE_FIND_ROOT_PATH_MODE_LIBRARY"] = "ONLY"
            tc.variables["CMAKE_FIND_ROOT_PATH_MODE_INCLUDE"] = "ONLY"
            tc.variables["CMAKE_FIND_ROOT_PATH_MODE_PACKAGE"] = "ONLY" # For find_package()

        tc.generate()

    def configure(self):
        if self.settings.arch == "armv7hf":
            if "opus" in self.options:
                self.options["opus"].with_asm = True
                self.options["opus"].with_neon = True

    def build_requirements(self):
        self.tool_requires("cmake/3.27.9")