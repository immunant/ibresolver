# Ibresolver

This is a QEMU TCG plugin for resolving indirect branches. The plugin takes in a list of addresses of indirect calls and jumps and produces a .csv with the addresses the branches resolved to.

# Building and prerequisites

To build, just run `make` from the root directory of this repo which should create the plugin, `libibresolver.so`. This plugin also requires a patched version of QEMU. To download and build QEMU do

```
$ git clone https://github.com/qemu/qemu

$ cd qemu

# The specific commit doesn't matter too much, but the patch is known to work with this one.
$ git checkout ecf2706e271fa705621f0d5ad9517fe15a22bf22

$ git apply /path/to/this/repo's/qemu.patch

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
$ /path/to/qemu -plugin ./libibresolver.so,arg="$CALLSITES_TXT",arg="$OUTPUT_CSV" $BINARY
```

Note that the argument to the `-plugin` flag is a comma-separated list with no spaces and the plugin must be a path (i.e. QEMU won't accept just `libibresolver.so`). Running QEMU on non-native binaries may require passing in the `-L` flag (e.g. `/path/to/qemu-arm -L /usr/arm-linux-gnueabihf/ -plugin ...`).

# Data formats
The list of callsites given as an input should have one address per line in hexdecimal prefixed by "0x". Only indirect calls originating in the binary passed to QEMU are currently supported. See the [arm32 test input](tests/arm32/fn_ptr.elf.txt) for an example.

The output is a csv with the address of the callsite in the first column, the address the indirect branch resolved to (given as an offset into the corresponding ELF image) in the second column and the name of the destination ELF image in the final column. Note that "binary" refers to the program passed to QEMU and "interpreter" refers to the system's ELF interpreter (typically `/lib64/ld-linux-x86-64.so.2`, `/lib/ld-linux-armhf.so.3` or similar). See the [arm32 test output](tests/arm32/fn_ptr.csv) for an example of the output.

# Supported architectures

This plugin currently works on x86-64 and arm32 binaries in user mode. It may also work in system mode and with binaries for other architectures though this hasn't been tested yet. Architectures with jump delay slots (e.g. MIPS, SPARC) and multithreaded programs are currently not expected to work.
