<div align="center">
   
# RPCSX
*An experimental emulator for PS4 (PlayStation 4) for Linux written in C++*

[![Build RPCSX](../../../actions/workflows/rpcsx.yml/badge.svg)](../../../actions/workflows/rpcsx.yml)

[![Formatting check](../../../actions/workflows/format.yml/badge.svg)](../../../actions/workflows/format.yml)

[![](https://img.shields.io/discord/252023769500090368?color=5865F2&logo=discord&logoColor=white)](https://discord.gg/t6dzA4wUdG)

</div>

> **Warning** <br/>
> It's NOT possible to run any games yet, and there is no ETA for when this will change.

> Do not ask for link to games or system files. Piracy is not permitted on the GitHub nor in the Discord.


## Contributing

If you want to contribute as a developer, please contact us in the [Discord](https://discord.gg/t6dzA4wUdG).

## Building

### The dependencies for Debian-like distributions:
```   
sudo apt install build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev vulkan-validationlayers-dev spirv-tools glslang-tools libspirv-cross-c-shared-dev
```

### The dependencies for Fedora distributions:

```
sudo dnf in cmake libunwind-devel glfw-devel vulkan-devel vulkan-validation-layers-devel spirv-tools glslang-devel gcc-c++ gcc spirv-tools-devel xbyak-devel
```

## Getting spriv-cross on Fedora and Arch Linux

```
git clone https://github.com/KhronosGroup/SPIRV-Cross && cd SPIRV-Cross && mkdir build && cd build && cmake .. && cmake --build . && sudo make install
```
> **Warning** <br/>
> Fedora will compile to a point and then error out

## Cloning the Repo

```
git clone --recursive https://github.com/RPCSX/rpcsx && cd rpcsx
```

> if you get a cmake error run
```
git submodule update --init --recursive
```
   
## How to Compile the emulator
   
```
mkdir -p build && cd build && cmake .. && cmake --build .
```

## How to create a virtual HDD

> The PS4 has case-insensitive filesystem. To create the virtual hdd do the following:
 
```
truncate -s 512M ps4-hdd.exfat
```

```
mkfs.exfat -n PS4-HDD ./ps4-hdd.exfat
```

```
mkdir ps4-fs
```

```
sudo mount -t exfat -o uid=`id -u`,gid=`id -g` ./ps4-hdd.exfat ./ps4-fs
```

## Running Samples and Games
   
See the Commands of `rpcsx-os` (`-h` argument), or join the [Discord](https://discord.gg/t6dzA4wUdG) for help.

You can run the emulator with Samples using this command:
   
```
rm -f /dev/shm/rpcsx-* && ./rpcsx-os --mount  "<path to fw>/system" "/system" --mount "<path to 'game' root>" /app0 /app0/some-test-sample.elf [<args for test elf>...]
```

## Creating a log

You can use this flag if you experience a segfault
    
```
--trace
``` 
    
You can redirect all log messages to a file by appending the following to the command:

```
&>log.txt
```
      


## License

RPCSX is licensed under GPLv2 license except directories containing their own LICENSE file, or files containing their own license.
Thus, orbis-kernel is licensed under the MIT license.

