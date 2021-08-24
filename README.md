# Ibresolver

This is a QEMU TCG plugin for resolving indirect branches. The plugin takes in a list of addresses of indirect calls and jumps and produces a .csv with a list of `callsite,destination` entries.

# Building and prerequisites

To build, just run `make` from the root directory of this repo which should create the plugin, `libibresolver.so`. This plugin also requires a patched version of QEMU. To download and build QEMU do

```
$ git clone https://github.com/qemu/qemu

$ cd qemu

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

See [demo/fn_ptr.elf.txt](demo/fn_ptr.elf.txt) for the expected format of the input list of callsites. Also note that the argument to the `-plugin` flag is a comma-separated list with no spaces and the plugin must be a path (i.e. QEMU won't accept just `libibresolver.so`).

# Supported architectures

This plugin currently works on x86-64 binaries in user mode. It may also work in system mode and with binaries for other architectures though this hasn't been tested yet. Architectures with jump delay slots (e.g. MIPS, SPARC) and multithreaded programs are currently not expected to work.
