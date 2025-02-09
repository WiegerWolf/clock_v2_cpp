from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout

class MyProject(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    # Remove CMakeToolchain from generators list
    generators = "CMakeDeps"  # Keep only CMakeDeps here

    def requirements(self):
        self.requires("sdl/2.28.3")
        self.requires("sdl_image/2.8.2")
        self.requires("sdl_ttf/2.24.0")
        self.requires("libcurl/8.11.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("cpp-httplib/0.18.3")

    def layout(self):
        cmake_layout(self)

    #  Keep CMakeToolchain instantiation and generation in generate()
    def generate(self):
        tc = CMakeToolchain(self)
        if self.settings.arch == "armv7hf":
            # ARM-specific flags for Raspberry Pi
            tc.variables["CMAKE_C_FLAGS"] = "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
            tc.variables["CMAKE_CXX_FLAGS"] = "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
        tc.generate()

    # Optional: Handle package-specific options
    def configure(self):
        if self.settings.arch == "armv7hf":
            # Example for opus package configuration (if needed)
            if "opus" in self.options:
                self.options["opus"].with_asm = True
                self.options["opus"].with_neon = True

    # Optional: Add build requirements like CMake
    def build_requirements(self):
        self.tool_requires("cmake/3.27.9")