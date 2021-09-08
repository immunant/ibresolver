# Overview

This is a QEMU user-mode plugin for resolving indirect branches. The plugin supports various architectures and uses a configurable disassembly backend to detect indirect jumps and calls. When the specified branches are taken, the callsite and destination are written to a .csv.

# Building and prerequisites

This plugin requires a patched version of QEMU. To download and build QEMU do

```
$ git clone https://github.com/qemu/qemu

$ cd qemu

# The specific commit doesn't matter too much, but the patch has been tested with this one.
$ git checkout ecf2706e271fa705621f0d5ad9517fe15a22bf22

$ git apply /path/to/this/repo's/qemu.patch

$ mkdir build

$ cd build

# To see available targets
$ ../configure --help

# To reduce compilation times build only the necessary targets
$ ../configure --target-list="$TARGETS"

# Or ninja
$ make
```

## Building the plugin

This plugin detects indirect branches with either a built-in backend or a custom one provided at runtime. By default `make` builds the plugin with a simple backend which only detects `blx` on 32-bit ARM and `callq` on x86-64. The other option is to use [binaryninja](https://binary.ninja/) to identify indirect branches.

### Building with binaryninja

After installing [binaryninja](https://docs.binary.ninja/getting-started.html), download the binaryninja API with the following

```
$ git submodule update
```

Then [build the binaryninja API](https://github.com/Vector35/binaryninja-api#build-instructions) and build this plugin with

```
make BACKEND=binja BINJA_INSTALL_DIR=/path/to/binaryninja/installation/
```

Note that `BINJA_INSTALL_DIR` is the path to binaryninja not the API.

### Building custom backends

Custom backends can be used by [building shared libraries](https://tldp.org/HOWTO/Program-Library-HOWTO/shared-libraries.html#AEN95) that define the following functions
```
// Checks if the backend supports the given architecture. Here `arch_name` is the suffix of the QEMU build (e.g. qemu-x86_64, qemu-arm).
extern "C" bool arch_supported(const char *arch_name);

// Checks if the given instruction is an indirect branch.
extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size);
```

Note that the name of the shared library should be prefixed by "lib" and have the extension ".so".

# Usage

To run QEMU with the plugin using the built-in backend do

```
$ /path/to/qemu -plugin ./libibresolver.so,arg="$OUTPUT_CSV" $BINARY
```

Note that the argument to the `-plugin` flag is a comma-separated list with no spaces and the plugin must be a path (i.e. QEMU won't accept just `libibresolver.so`). Running QEMU on non-native binaries may require passing in the `-L` flag (e.g. `/path/to/qemu-arm -L /usr/arm-linux-gnueabihf/ -plugin ...`).

To use a custom backend add the path to the shared library as the second plugin argument. For example to use a shared library named `libbackend.so` as the backend use
```
$ /path/to/qemu -plugin ./libiresolver.so,arg="$OUTPUT_CSV",arg="./libbackend.so" $BINARY
```

# Output format

The output is a csv formatted as follows
```
callsite offset,callsite image,destination offset,destination image
```

Where each line has the callsite and destination of each indirect branch. Instead of outputting the virtual addresses (vaddrs) of callsites and destinations, this plugin translates those vaddrs to offsets in the corresponding ELFs. The output can then easily be interpreted by running `objdump` on one of the ELFs in the second or fourth columns and looking for the corresponding offset address.

# Supported architectures

This plugin currently works on x86-64 and arm32 binaries. Support for other architectures may be added through custom disassembly backends, though this has not been tested yet. Architectures with jump delay slots (e.g. MIPS, SPARC), multithreaded programs and JITs are currently not expected to work.
