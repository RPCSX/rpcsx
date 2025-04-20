## Building


### The dependencies for Debian-like distributions.
```   
sudo apt install build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev libsox-dev git libasound2-dev nasm g++-14
```

### The dependencies for Fedora distributions:

```
sudo dnf install cmake libunwind-devel glfw-devel vulkan-devel gcc-c++ gcc sox-devel alsa-lib-devel nasm
```

### The dependencies for Arch distributions:

```
sudo pacman -S libunwind glfw-x11 vulkan-devel sox git cmake alsa-lib nasm
```

### The dependencies for MacOS (iOS target, rpcs3 only):

macOS 15, Xcode 16 (with iOS workload) and Cmake 3.20+

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

For iOS you need to manually specify the compiler and toolchain file:

```
cmake -B build -G Xcode -DCMAKE_TOOLCHAIN_FILE=ios/ios.toolchain.cmake -DPLATFORM=OS64 -DCMAKE_POLICY_VERSION_MINIMUM=3.20
cmake --build build --config Release
```