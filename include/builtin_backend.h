#ifndef BUILTIN_BACKEND_H
#define BUILTIN_BACKEND_H

bool arch_supported_default_impl(const char *arch_name);
bool is_indirect_branch_default_impl(uint8_t *insn_data, size_t insn_size);

#endif
