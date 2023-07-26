# Usage

## How to prepare eboot.bin with ps4_unfself

### eboot.bin needs to be unfselfed and to do this [python2](BUILDING.md) is required with this tool [ps4_unfself](https://github.com/SocraticBliss/ps4_unfself)

Place ```ps4_unfself.py``` into the same folder as the eboot.bin then open a console and type 

```
python2 ps4_unfself.py eboot.bin
```
you will get a read out and it will make a ```eboot.elf```

## How to run samples and games

You will need firmware 5.05 dumped via PS4 FTP it must be fully decrypted and we do not provide the firmware

See the Commands of `rpcsx-os` (`-h` argument), or join the [Discord](https://discord.gg/t6dzA4wUdG) for help.

You can run the emulator with some samples using this command:

```sh
rm -f /dev/shm/rpcsx-* && ./rpcsx-os --mount  "<path to fw>/system" "/system" --mount "<path to 'game' root>" /app0 /app0/some-test-sample.elf [<args for test elf>...]
```

## Creating a log

### You can use this flag if you encountered a segfault for debugging purposes.

```sh
--trace
```

### You can redirect all log messages to a file by appending this command:

```sh
&>log.txt
```
