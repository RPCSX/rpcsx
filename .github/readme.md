<div align="center">
   
# RPCSX
*An experimental emulator for PS4 (PlayStation 4) for Linux written in C++*

[![Build RPCSX](../../../actions/workflows/rpcsx.yml/badge.svg)](../../../actions/workflows/rpcsx.yml)

[![Formatting check](../../../actions/workflows/format.yml/badge.svg)](../../../actions/workflows/format.yml)

[![](https://img.shields.io/discord/252023769500090368?color=5865F2&logo=discord&logoColor=white)](https://discord.gg/t6dzA4wUdG)

</div>

> **Warning** <br/>
> It's NOT possible to run any games yet, and there is no ETA for when this will change


## Contributing

If you want to contribute as a developer, please contact us in the [Discord](https://discord.gg/t6dzA4wUdG).

## Building

1. Install dependencies for Debian-like distributions
   
   `sudo apt install build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev vulkan-validationlayers-dev spirv-tools glslang-tools libspirv-cross-c-shared-dev libxbyak-dev`
   
3. Building
   
   `mkdir -p build && cd build && cmake .. && cmake --build .`

4. How to create to create virtual hdd

   Setup size of virtual hdd
 
   `truncate -s 512M ps4-hdd.exfat`

   `mkfs.exfat -n PS4-HDD ./ps4-hdd.exfat`

    `mkdir ps4-fs`

    ``sudo mount -t exfat -o uid=`id -u`,gid=`id -g` ./ps4-hdd.exfat ./ps4-fs``

5. Run
   
     See usage message of `rpcsx-os` (`-h` argument), or join the [Discord](https://discord.gg/t6dzA4wUdG) for help.

     you can run emulator with Samples using this command:
   
     `rm -f /dev/shm/rpcsx-* && ./rpcsx-os --mount  "<path to fw>/system" "/system" --mount "<path to 'game' root>" /app0 /app0/some-test-sample.elf [<args for test elf>...]`

 6. Creating a log

    use flag
    
    `--trace` if you got sigfault
    
    you can redirect all log messages to the file by appending following to command:

     `&>log.txt`
      


## License

RPCSX is licensed under GPLv2 license except directories containing their own LICENSE file, or files containing their own license.
Thus, orbis-kernel is licensed under the MIT license.
