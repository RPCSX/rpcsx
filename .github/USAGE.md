# Usage

## How to run samples and games

See the Commands of `rpcsx-os` (`-h` argument), or join the [Discord](https://discord.gg/t6dzA4wUdG) for help.

You can run the emulator with some samples using this command:

```sh
rm -f /dev/shm/rpcsx-* && ./rpcsx-os --mount  "<path to fw>/system" "/system" --mount "<path to 'game' root>" /app0 /app0/some-test-sample.elf [<args for test elf>...]
```

## Creating a log

You can use this flag if you encountered a segfault for debugging purposes.

```sh
--trace
```

You can redirect all log messages to a file by appending this command:

```sh
&>log.txt
```
