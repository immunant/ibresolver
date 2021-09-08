#pragma once
#include <cstdint>
#include <cstdlib>

// Checks if the backend supports the given architecture. Here `arch_name` is the suffix of the QEMU build (e.g. qemu-x86_64, qemu-arm).
extern "C" bool arch_supported_default_impl(const char *arch_name);

// Checks if the given instruction is an indirect branch.
extern "C" bool is_indirect_branch_default_impl(uint8_t *insn_data, size_t insn_size);
