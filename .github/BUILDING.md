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

## Cloning the Repo

```
git clone --recursive https://github.com/RPCSX/rpcsx && cd rpcsx
```
## How to compile the emulator
   
```
cmake -B build && cmake --build build -j$(nproc)
```
