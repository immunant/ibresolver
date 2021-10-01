# Indirect branch resolver design

This plugin works by registering callbacks for when QEMU executes indirect branches and all their
potential destinations. Let's call these the "execution callbacks". Each time [QEMU translates a
block of instructions](https://qemu.readthedocs.io/en/latest/devel/tcg.html#translator-internals)
`block_trans_handler` in `plugin.cpp` decides which execution callbacks to register for which
instructions. Each block is assumed to have [one entry
point](https://qemu.readthedocs.io/en/latest/devel/tcg.html#direct-block-chaining) (i.e. the first
instruction) and [multiple potential exit
points](https://github.com/qemu/qemu/blob/master/tcg/README#L78-L79) (i.e. the last instruction or
any conditional jumps/calls).

Since QEMU can't jump into the middle of a block, the destination of any indirect branch that's
taken must be the first instruction in a block. So for the first instruction in each block we
register the `branch_taken` execution callback to write to the output file if the previous
instruction was an indirect branch. To check that condition, the `indirect_branch_exec` execution
callback is registered for all indirect branches. This callback sets the `optional<uint64_t>
branch_addr` variable to the callsite address each time an indirect branch is executed. The
`branch_taken` callback then uses that variable along with the destination address to write a line
to the output file.

Since indirect branches may be conditional we register the `branch_skipped` execution callback for
the instruction following an indirect branch if it falls within the same block. If these
instructions are executed it means that the branch was not taken so the callback clears the
`branch_addr` variable.

Indirect branches may also be the destination of another branch (e.g. if it's the first instruction
in a block). For these cases the `indirect_branch_at_start` callback is registered to ensure that
the two corresponding callbacks are executed in the correct order (first `branch_taken` then
`indirect_branch_exec`).


# Interpreting the callsite and destination addresses

All addresses passed to the callbacks are virtual addresses (vaddrs) of the **emulated**, or guest,
process. Outputting this is ok for statically-compiled binaries, but it might not be that useful for
dynamically-linked programs without knowledge of the memory map at runtime. To make the output
easier to interpret, we also show the addresses as offsets into the corresponding ELF files.

The first step to going from guest vaddrs to ELF file offsets is to convert to a host vaddr.
Here host vaddrs are the ones corresponding to the QEMU process itself. We do this conversion by
adding the `guest_base` offset exposed by our QEMU patch.

The next step is to find the corresponding ELF file. To do this the branch resolver plugin parses
the output of `/proc/self/maps`. Since QEMU plugins are just shared libraries, we'll see the memory
mapped by QEMU for the guest process in `/proc/self/maps`. We parse `/proc/self/maps` each time a
branch is taken to avoid having to internally track the memory mappings (this is very complicated as
seen with the initial syscall-tracing approach).

The line in `/proc/self/maps` that contains the host vaddr corresponds to the loadable segment in
our ELF file. We take our host vaddr and subtract the segment's lowest vaddr to get an offset into
the segment. Then since segment vaddrs may not always correspond to file offsets, we add the
segment's file load offset to get the vaddr as an offset into the ELF. To distinguish offsets into
different files we also output the name of the callsite and destination ELFs in the output.

# Alternatives considered

We considered various approaches for computing ELF offsets from guest vaddrs and how they might
affect our ability to support other architectures and their performance. Instead of parsing
`/proc/self/maps` as described above, we initially traced syscalls made by the dynamic linker and
internally kept track of what ELFs were loaded in memory and where. This approach quickly became
unwieldy as the syscalls can be architecture-specific (e.g. x86-64 uses mmap, ARM32 uses mmap2) and
keeping our internal representation of the memory map consistent proved to be a recurring issue for
complex programs. Instead we decided to go with a more architecture-agnostic approach by checking
the system's memory map directly via `/proc/self/maps`.

A subtle point, that is implicit in the previous section, is that the plugin parses the maps file
every time an indirect jump is taken. While this may incur a slight performance hit for programs
that heavily use indirect control flow, we intentionally avoided caching the maps file for
simplicity. Caching would entail tracing a subset of the dynamic linker's syscalls (e.g. when a
shared object is loaded/unloaded) to determine when to update the cache which would make the plugin
significantly more complex. Since we don't intend to support every combination of architecture and
dynamic linker, caching the maps file is possbile if performance becomes an issue.

Another place where we made a decision that may affect performance is in how we write to the output
file. We currently write to the output file as the guest program is emulated.  For programs that
make heavy use of indirect control flow it may be more efficient to buffer the output in memory and
write it to a file at the end. However, this would require modifying the emulated programs so that
they terminate. This would be required to ensure that the QEMU plugin executed the "finalize"
callback that would write the output to a file. Instead we decided to avoid the need to modify the
emulated programs at the expense of potentially incurring a slight performance hit in rare cases.
