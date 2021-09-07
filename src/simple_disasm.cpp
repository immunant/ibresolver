#include <string>
#include <cstring>

#include "disasm.h"

using namespace std;

#if BACKEND == SIMPLE_BACKEND

bool init_backend_impl(const char *arch_name);
bool is_indirect_branch_impl(uint8_t *insn_data, size_t insn_size);

extern "C" bool init_backend(const char *arch_name) { return init_backend_impl(arch_name); }

extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size) {
    return is_indirect_branch_impl(insn_data, insn_size);
}

#endif

static string arch = "";

bool init_backend_impl(const char *arch_name) {
    if (strcmp(arch_name, "arm") && strcmp(arch_name, "x86_64")) {
        return false;
    }
    arch = string(arch_name);
    return true;
}

bool is_indirect_branch_impl(uint8_t *insn_data, size_t insn_size) {
    if (!arch.compare("arm")) {
        // Handles blx rn
        const uint32_t blx_variable = 0xf000000f;
        const uint32_t blx = 0xf12fff3f;
        if (insn_size == 4) {
            uint32_t b0 = insn_data[0];
            uint32_t b1 = insn_data[1];
            uint32_t b2 = insn_data[2];
            uint32_t b3 = insn_data[3];
            uint32_t word = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
            // Set variable bitfields in the instruction
            word |= blx_variable;
            if (word == blx) {
                return true;
            }
        }
    } else if (!arch.compare("x86_64")) {
        // Handles callq rax, rcx, rdx, etc.
        if (insn_size == 2) {
            uint8_t b0 = *insn_data;
            uint8_t b1 = *(insn_data + 1);
            if ((b0 == 0xff) && (0xd0 <= b1) && (b1 <= 0xd6)) {
                return true;
            }
        }
        // Handles callq r8, r9, r10, etc.
        if (insn_size == 3) {
            uint8_t b0 = *insn_data;
            uint8_t b1 = *(insn_data + 1);
            uint8_t b2 = *(insn_data + 2);
            if ((b0 == 0x41) && (b1 == 0xff) && (0xd0 <= b2) && (b2 <= 0xd6)) {
                return true;
            }
        }
    }
    return false;
}
