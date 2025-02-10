## Deps

```bash
sudo apt update
sudo apt install -y git ssh
git clone git clone git@github.com:WiegerWolf/clock_v2_cpp.git
```

```bash
sudo apt install -y cmake gcc make g++ libsdl2-dev \
    libsdl2-image-dev libsdl2-ttf-dev libcurl4-openssl-dev
```

## Build Steps

```bash
mkdir build
cd build
cmake .. 
make
```
