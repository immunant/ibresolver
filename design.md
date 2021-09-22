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

All addresses passed to the callbacks are virtual addresses (vaddrs) of the emulated process. This
is ok for statically-compiled binaries, but it might not be that useful for dynamically-linked
programs without knowledge of the memory map at runtime. To make the output more useful, we also
show the addresses as offsets into the corresponding ELF files.

The first step to going from **emulated** vaddrs to ELF file offsets is to convert to a real vaddr.
Here real vaddrs are the ones corresponding to the QEMU process itself. We do this conversion by
adding the `guest_base` offset exposed by our QEMU patch.

The next step is to find the corresponding ELF file. To do this the branch resolver plugin parses
the output of `/proc/self/maps`. Since QEMU plugins are just shared libraries, we'll see the memory
mapped by QEMU for the emulated process in `/proc/self/maps`. We parse `/proc/self/maps` each time a
branch is taken to avoid having to internally track the memory mappings (this is very complicated as
seen with the initial syscall-tracing approach).

The line in `/proc/self/maps` that contains the real vaddr corresponds to the loadable segment in
our ELF file. We take our real vaddr and subtract the segment's lowest vaddr to get an offset into
the segment. Then since segment vaddrs may not always correspond to file offsets for
dynamically-linked programs we add the segment's file load offset to get the vaddr as an offset into
the ELF. To distinguish offsets into different files we also output the name of the callsite and
destination ELFs in the output.
