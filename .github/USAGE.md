# Usage

## How to run samples and games

You will need firmware 5.05 dumped via PS4 FTP it must be fully decrypted and we do not provide the firmware

See the Commands of `rpcsx` (`-h` argument), or join the [Discord](https://discord.gg/t6dzA4wUdG) for help.

You can run the emulator with some samples using this command:

```sh
./rpcsx --mount  "<path to fw>/system" "/system" --mount "<path to 'game' root>" /app0 /app0/some-test-sample.elf [<args for test elf>...]
```
### You can now enter safe mode 

```sh
./rpcsx --system --safemode --mount $PATH_TO_YOUR_FW_ROOT / /mini-syscore.elf
```
drop ```--safemode``` to have normal mode (not expected to produce graphics yet)
## Creating a log

### You can use this flag if you encountered a segfault for debugging purposes.

```sh
--trace
```

### You can redirect all log messages to a file by appending this command:

```sh
&>log.txt
```
