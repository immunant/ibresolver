# Ibresolver

This is a QEMU TCG plugin for resolving indirect branches. The plugin takes in a list of addresses of indirect calls and jumps and produces a .csv with a list of `callsite,destination` entries.

# Building and prerequisites

To build, just run `make` from the root directory of this repo which should create the plugin, `libibresolver.so`. Although this plugin is not built against a particular version of QEMU, plugins have only [recently become enabled by default](https://github.com/qemu/qemu/commit/ba4dd2aabc35bc5385739e13f14e3a10a223ede0) in QEMU so most distros' packages do not support plugins. If your system's QEMU gives the error `qemu: unknown option 'plugin'` you must compile QEMU from source.


## Building QEMU

```
$ git clone https://github.com/qemu/qemu

$ cd qemu

$ mkdir build

$ cd build

# To see available targets
$ ../configure --help

# To reduce build times list the necessary targets
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

This plugin currently works on statically linked x86-64 binaries in user mode. It may also work with statically linked binaries for other architectures though none have been tested yet. Architectures with jump delay slots, like MIPS, and dynamically linked binaries are currently not expected to work.
