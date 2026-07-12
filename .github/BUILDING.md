## Building


### The dependencies for Debian-like distributions.
```   
sudo apt install build-essential cmake pkg-config libunwind-dev libsox-dev git libasound2-dev nasm g++-14 \
  libudev-dev libxcb1-dev libx11-dev libwayland-dev libxkbcommon-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libxext-dev
```

Without `pkg-config` and `libudev-dev` the configure step fails (Vulkan-Loader and hidapi respectively); without the X11/Wayland headers Vulkan-Loader and GLFW are built without window-system support. This list matches what CI installs (`.github/workflows/rpcsx.yml`).

### The dependencies for Fedora distributions:

```
sudo dnf install cmake libunwind-devel gcc-c++ gcc sox-devel alsa-lib-devel nasm
```

### The dependencies for Arch distributions:

```
sudo pacman -S libunwind sox git cmake alsa-lib nasm
```

## Cloning the Repo

```
git clone --recursive https://github.com/RPCSX/rpcsx && cd rpcsx
```
## How to compile the emulator
   
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_INIT="-march=native" && cmake --build build -j$(nproc)
```

For Ubuntu 24.04 you need to manually specify compiler version:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_INIT="-march=native" -DCMAKE_CXX_COMPILER=g++-14 && cmake --build build -j$(nproc)
```

### Building with clang

clang 20 or newer is required: clang 18 rejects the codebase's C++23 relaxed `constexpr` (P2448, e.g. in `rx/StaticString.hpp` and `rpcsx/gpu/lib/gcn-shader/src/eval.cpp`), and clang 19 fails on the enum-reflection code in `rx/format.hpp` (its `-Wenum-constexpr-conversion` diagnostic is not SFINAE-friendly). The matching `clang-tools` package is also needed, since CMake invokes `clang-scan-deps` for dependency scanning:

```
sudo apt install clang-20 clang-tools-20
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_INIT="-march=native" -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 && cmake --build build -j$(nproc)
```
