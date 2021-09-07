# Indirect branch resolver design

The ibresolver plugin works by registering callbacks for when QEMU executes indirect branches and
all their potential destinations. The execution callbacks are registered in `block_trans_handler`
each time a block of instructions is translated by QEMU. Each translation block is assumed to have
one entry point (i.e. the first instruction) and various potential exit points (i.e. the last
instruction or any conditional jumps/calls).

Under these assumptions the destination of any indirect branch must be the start of a translation
block, so the `branch_taken` callback is registered for all the first instructions. This callback
writes the callsite and destination addresses to the output file if the previous instruction was an
indirect branch. To check this condition, the `indirect_branch_exec` callback is registered for all
indirect jumps and calls. Since indirect branches may be conditional, the `branch_skipped` callback is
registered for the following instructions (if any) to clear the "branch taken" flag (the
`optional<uint64_t> branch_addr`). When an indirect branch may also be the destination of another
branch (i.e. it's the first in the block), the `indirect_branch_at_start` callback is registered to
ensure that the two corresponding callbacks are executed in the correct order.

All addresses passed to the callbacks are virtual addresses (vaddrs) of the emulated process. To map
these vaddrs to offsets into the ELF images (the target program, ld.so, other shared libraries) this
plugin looks at `/proc/self/maps` each time an indirect branch is taken. Parsing `/proc/self/maps`
at each branch allows the plugin to work regardless of what shared libraries are mapped or unmapped
to memory while avoiding the need to trace the runtime linker and trying to track the memory mapping
in the plugin. Currently the output is specified in terms of offsets into ELF images, but it'd be
nice to make the output match the addresses displayed by objdump.
