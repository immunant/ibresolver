# Ibresolver

This is a QEMU TCG plugin for resolving indirect branches. The plugin takes in a list of addresses of indirect calls and jumps and produces a .csv with the addresses the branches resolved to.

# Building and prerequisites

To build, just run `make` from the root directory of this repo which should create the plugin, `libibresolver.so`. Although this plugin is not built against a particular version of QEMU, plugins have only [recently become enabled by default](https://github.com/qemu/qemu/commit/ba4dd2aabc35bc5385739e13f14e3a10a223ede0) in QEMU so most distros' packages do not support plugins. If your system's QEMU gives the error `qemu: unknown option 'plugin'` you must compile QEMU from source.

```
$ git clone https://github.com/qemu/qemu

$ cd qemu

$ mkdir build

$ cd build

# To see available targets
$ ../configure --help

# To reduce compilation times build only the necessary targets
$ ../configure --target-list="$TARGETS"

$ make
```

# Usage

To run QEMU with the plugin use

```
$ /path/to/qemu -plugin ./libibresolver.so,arg="$CALLSITES_TXT",arg="$OUTPUT_CSV",arg="/absolute/path/to/$BINARY" $BINARY
```

Note that the argument to the `-plugin` flag is a comma-separated list with no spaces and the plugin must be a path (i.e. QEMU won't accept just `libibresolver.so`). Running QEMU on non-native binaries may require passing in the `-L` flag (e.g. `/path/to/qemu-arm -L /usr/arm-linux-gnueabihf/ -plugin ...`).

# Data formats
The list of callsites given as an input should have one address per line in hexdecimal prefixed by "0x". Only indirect calls originating in the binary passed to QEMU are currently supported. See the [arm32 test input](tests/arm32/fn_ptr.elf.txt) for an example.

The output is a csv with the address of the callsite in the first column, the address the indirect branch resolved to (given as an offset into the corresponding ELF image) in the second column and the name of the destination ELF image in the final column. Note that "binary" refers to the program passed to QEMU and "interpreter" refers to the system's ELF interpreter (typically `/lib64/ld-linux-x86-64.so.2`, `/lib/ld-linux-armhf.so.3` or similar). See the [arm32 test output](tests/arm32/fn_ptr.csv) for an example of the output.

# Supported architectures

This plugin currently works on native binaries in user mode. Support for non-native binaries (e.g. arm32 on running on x86-64) is currently a work-in-progress. Architectures with jump delay slots (e.g. MIPS, SPARC) and multithreaded programs are currently not expected to work.
