# Building with Conan

## Prerequisites
- CMake (3.15 or higher)
- Conan package manager
- C++ compiler
- X11 development libraries (on Ubuntu/Debian):
```bash
sudo apt-get install -y libx11-xcb-dev libfontenc-dev libxaw7-dev libxkbfile-dev \
  libxmu-dev libxmuu-dev libxpm-dev libxres-dev libxcb-glx0-dev \
  libxcb-render-util0-dev libxcb-xkb-dev libxcb-icccm4-dev \
  libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
  libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
  libxcb-xinerama0-dev libxcb-dri3-dev libxcb-cursor-dev \
  libxcb-dri2-0-dev libxcb-present-dev libxcb-composite0-dev \
  libxcb-ewmh-dev libxcb-res0-dev libxcb-util-dev libxcb-util0-dev cmake
```

## Build Steps

1. Create and enter build directory:
```bash
mkdir build && cd build
```

2. Install dependencies using Conan:
```bash
conan install .. --build=missing
```

3. Configure CMake:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

4. Build the project:
```bash
cmake --build .
```

## Notes
- Make sure your `conanfile.txt` or `conanfile.py` is properly configured
- For development, you can use Debug build type instead of Release
- Run `conan profile detect` if you haven't set up a Conan profile yet