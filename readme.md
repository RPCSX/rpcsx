RPCSX
=====
This is an experimental emulator for PS4 (Play Station 4) written in C++ for Linux.
It's NOT possible to run any games yet. It's also unknown when it will become possible.

## Contributing

If you want to contribute as a developer, please contact us in the Discord. https://discord.gg/PYUcckGr

## Building

1. Install dependencies for Debian-like distributions
   
   `sudo apt install build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev vulkan-validationlayers-dev spirv-tools glslang-tools`
2. Building
   
   `mkdir -p build && cd build && cmake .. && cmake --build .`

4. Run
   
   See usage message of rpcsx-os (-h argument), or join the Discord for help.

## License

RPCSX is licensed under GPLv2 license except directories containing their own LICENSE file, or files containing their own license.
Thus, orbis-kernel is licensed under the MIT license.
