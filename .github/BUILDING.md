## Building


### The dependencies for Debian-like distributions.

```   
sudo apt install build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev vulkan-validationlayers-dev spirv-tools glslang-tools libspirv-cross-c-shared-dev libsox-dev git libasound2-dev
```
# git is only needed for ubuntu 22.04

### The dependencies for Fedora distributions:

```
sudo dnf install cmake libunwind-devel glfw-devel vulkan-devel vulkan-validation-layers-devel spirv-tools glslang-devel gcc-c++ gcc spirv-tools-devel xbyak-devel sox-devel alsa-lib-devel
```

### The dependencies for Arch distributions:

```
sudo pacman -S libunwind glfw-x11 vulkan-devel sox glslang git cmake alsa-lib
```
> Side note you will need to pull ``spirv-cross`` from the AUR for now so do the following
```
sudo pacman -S --needed git base-devel && git clone https://aur.archlinux.org/yay.git && cd yay && makepkg -si
```
```
yay -S spirv-cross
```
## Getting spriv-cross on Fedora

```
git clone https://github.com/KhronosGroup/SPIRV-Cross && cd SPIRV-Cross && mkdir build && cd build && cmake .. && cmake --build . && sudo make install
```
> [!WARNING]
> Fedora will compile to a point and then error out

## Cloning the Repo

```
git clone --recursive https://github.com/RPCSX/rpcsx && cd rpcsx
```
## How to compile the emulator
   
```
cmake -B build && cmake --build build -j$(nproc)
```
