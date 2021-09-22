# Overview

This is a QEMU user-mode plugin for resolving indirect branches. The plugin supports various architectures and uses a configurable disassembly backend to detect indirect jumps and calls. When a branch found by the backend is taken, the callsite and destination are written to a .csv.

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

# To reduce compilation times specify only the necessary targets as a comma-separated list.
# Built-in backends support `arm-linux-user` and `x86_64-linux-user`.
$ ../configure --target-list="$TARGETS"

# Or ninja
$ make
```

## Building the plugin

This plugin detects indirect branches with either a built-in disassembly backend or a custom one provided at runtime. By default `make` builds the plugin with the simple backend which only detects `blx` on 32-bit ARM and `callq` on x86-64. The other build-time option is to use [binaryninja](https://binary.ninja/) to identify indirect branches. Custom backends are specified as command line arguments when starting QEMU and can be used with either build option.

### Building with binaryninja

After installing [binaryninja](https://docs.binary.ninja/getting-started.html), download the binaryninja API with the following

```
$ git submodule update
```

Then [build the binaryninja API](https://github.com/Vector35/binaryninja-api#build-instructions) and build this plugin with

```
make BACKEND=binja BINJA_INSTALL_DIR=/path/to/binaryninja/installation/ BINJA_API_DIR=/path/to/binaryninja/API/build/out/
```

where `BINJA_INSTALL_DIR` is the path to the binaryninja installation which should have `libbinaryninjacore.so` and `BINJA_API_DIR` is the path to the binaryninja API build output directory which should have `libbinaryninjaapi.a`.

### Building custom backends

Custom backends can be made by [building shared libraries](https://tldp.org/HOWTO/Program-Library-HOWTO/shared-libraries.html#AEN95) that define the following functions
```
// Checks if the backend supports the given architecture. This function is only called when
// initializing the plugin. Here `arch_name` is the suffix of the QEMU build (e.g. "x86_64" for
// qemu-x86_64, "arm" for qemu-arm).
extern "C" bool arch_supported(const char *arch_name);

// Checks if the given instruction is an indirect branch. This is only called each time QEMU
// translates an instruction, not every time it's executed.
extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size);
```

Note that the name of the shared library should be prefixed by "lib" and have the file extension ".so". Building shared libraries requires passing the `-shared` and `-fPIC` flags to the compiler and possibly `-l`, `-L`, or `-Wl,-rpath=` depending on what it links against (e.g. if using C++ pass `-lstdc++`). See the link above for more details.

Custom backends may also fallback to the built-in backends by including [`include/builtin_backend.h`](include/builtin_backend.h) and linking against `libibresolver.so`. The build process for this would typically look like this
```
$ gcc my_source.c -shared -fPIC -I /path/to/this/repo/include/ -libresolver -L /path/to/this/repo/ -Wl,-rpath=/path/to/this/repo/ -o libmybackend.so
```

where the `-l`, `-L` and `-Wl,-rpath=` flags are used to link against the branch resolver plugin to get access to the built-in backends.

For an example of a custom backend see [`backend_demo.c`](backend_demo.c). This only finds indirect calls (like the simple backend), prints to stdout if a call is found and falls back to the built-in backend for all other instructions. To build this backend use `make demo` and pass the resulting `libdemo.so` to QEMU as described below.

# Usage

To run QEMU with the plugin using the built-in backend use

```
$ /path/to/qemu -plugin ./libibresolver.so,arg="$OUTPUT_CSV" $BINARY
```

Note that the argument to the `-plugin` flag is a comma-separated list with no spaces and the plugin must be a path (i.e. QEMU won't accept just `libibresolver.so`). Running QEMU on non-native binaries may require passing in the `-L` flag (e.g. `/path/to/qemu-arm -L /usr/arm-linux-gnueabihf/ -plugin ...`).

To use a custom backend add the path to the shared library as the second plugin argument. For example to use a shared library named `libdemo.so` as the backend use
```
$ /path/to/qemu -plugin ./libiresolver.so,arg="$OUTPUT_CSV",arg="./libdemo.so" $BINARY
```

# Output format

The output is a csv formatted as follows
```
callsite offset,dest offset,callsite vaddr,dest vaddr,callsite ELF,dest ELF
```

where each line has the callsite and destination of every indirect branch in the order they were taken. The columns labeled `offset` show the callsite and destination addresses as offsets into their corresponding ELF files. The columns labeled `vaddr` shows the callsite and destination as virtual addresses in the emulated process. To interpret the results (i.e. see what instructions are at/around the callsite and destination) use `objdump -d -F $BINARY` and search for the file offset of interest. For statically-compiled binaries `-F` is not strictly necessary since the vaddrs in the output will always correspond to the addresses shown on the left-hand side with just `objdump -d`. For dynamically-linked executables the file offset will often but not always correspond to the addresses shown in objdump which is why the `-F` may be required.

# Supported architectures

This plugin currently works on x86-64 and arm32 binaries. Support for other architectures may be added through custom disassembly backends, though this has not been tested yet. Architectures with jump delay slots (e.g. MIPS, SPARC), multithreaded programs and JITs are currently not expected to work.
