RPCSX
=====
This is an experimental emulator for PS4 (Play Station 4) written in C++ for Linux.
It's NOT possible to run any games yet. It's also unknown when it will become possible.

## Contributing

If you want to contribute as a developer, please contact us in the Discord. https://discord.gg/PYUcckGr

## Building

1. Install dependencies
   
   `sudo apt install clang spirv-tools glslang-tools libunwind-dev libglfw3 libglfw3-dev`
3. Install [Vulkan SDK](https://vulkan.lunarg.com/doc/sdk/latest/linux/getting_started.html)
4. Build Makefiles
   
   `cmake .`
5. Build RPCSX
   
   `make`

## License

RPCSX is licensed under GPLv2 license except directories containing their own LICENSE file, or files containing their own license.
Thus, orbis-kernel is licensed under the MIT license.
