# Building with Conan

## Prerequisites
- CMake (3.15 or higher)
- C++ compiler
- Development libraries (on Ubuntu/Debian):
```bash
sudo apt-get install -y libx11-xcb-dev libfontenc-dev libxaw7-dev libxkbfile-dev \
  libxmu-dev libxmuu-dev libxpm-dev libxres-dev libxcb-glx0-dev \
  libxcb-render-util0-dev libxcb-xkb-dev libxcb-icccm4-dev \
  libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
  libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
  libxcb-xinerama0-dev libxcb-dri3-dev libxcb-cursor-dev \
  libxcb-dri2-0-dev libxcb-present-dev libxcb-composite0-dev \
  libxcb-ewmh-dev libxcb-res0-dev libxcb-util-dev libxcb-util0-dev cmake \
  libsdl2-image-dev libsdl2-ttf-dev libcpp-httplib-dev libcurl4-openssl-dev
```

## Build Steps

```bash
cd build
cmake .. 
make
```

### Cross compile for RPi

Dependencies:

```bash
sudo dpkg --add-architecture armhf
sudo apt-get update
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    libsdl2-dev:armhf libsdl2-image-dev:armhf \
    libsdl2-ttf-dev:armhf libcurl4-openssl-dev:armhf libssl-dev:armhf
```

Build cmd:

```bash
mkdir build-rpi
cd build-rpi
cmake .. -DCMAKE_TOOLCHAIN_FILE=../rpi_toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Release \
         -DFETCHCONTENT_FULLY_DISCONNECTED=OFF
make
```