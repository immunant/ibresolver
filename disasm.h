#pragma once
#include <cstdlib>
#include <cstdint>

extern "C" bool init_backend(const char *arch_name);

extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size);
