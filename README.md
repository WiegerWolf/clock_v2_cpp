# Building with Conan

## Prerequisites
- CMake (3.15 or higher)
- Conan package manager
- C++ compiler

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