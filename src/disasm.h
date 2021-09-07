#pragma once
#include <cstdint>
#include <cstdlib>

extern "C" bool init_backend(const char *arch_name);

extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size);

// The simple backend impl functions are temporarily being used to find indirect branches that binja
// isn't reporting and don't need to be in the header if the simple backend is being used
#if BACKEND == BINJA_BACKEND

bool init_backend_impl(const char *arch_name);

bool is_indirect_branch_impl(uint8_t *insn_data, size_t insn_size);

#endif
