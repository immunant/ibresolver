#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum arch_t {
    arm,
    x86_64,
    unknown,
} arch_t;

static arch_t arch = unknown;

extern bool arch_supported(const char *arch_name) {
    if (!strcmp(arch_name, "arm")) {
        arch = arm;
        return true;
    }
    if (!strcmp(arch_name, "x86_64")) {
        arch = x86_64;
        return true;
    }
    return false;
}

extern bool is_indirect_branch(uint8_t *insn_data, size_t insn_size) {
    if (arch == arm) {
        // Handles blx rn
        const uint32_t blx_variable_bits = 0xf000000f;
        const uint32_t blx_constant_bits = 0x012fff30;
        // Arbitrarily set all variable bits in the blx instruction before comparing with the input instruction
        const uint32_t blx = blx_constant_bits | blx_variable_bits;
        if (insn_size == 4) {
            uint32_t b0 = insn_data[0];
            uint32_t b1 = insn_data[1];
            uint32_t b2 = insn_data[2];
            uint32_t b3 = insn_data[3];
            uint32_t word = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
            // Set all variable bits in the instruction
            word |= blx_variable_bits;
            if (word == blx) {
                printf("Found a `blx` instruction\n");
                return true;
            }
        }
    } else if (arch == x86_64) {
        // Handles callq rax, rcx, rdx, etc.
        if (insn_size == 2) {
            uint8_t b0 = insn_data[0];
            uint8_t b1 = insn_data[1];
            if ((b0 == 0xff) && (0xd0 <= b1) && (b1 <= 0xd6)) {
                printf("Found a `callq` instruction\n");
                return true;
            }
        }
        // Handles callq r8, r9, r10, etc.
        if (insn_size == 3) {
            uint8_t b0 = insn_data[0];
            uint8_t b1 = insn_data[1];
            uint8_t b2 = insn_data[2];
            if ((b0 == 0x41) && (b1 == 0xff) && (0xd0 <= b2) && (b2 <= 0xd6)) {
                printf("Found a `callq` instruction\n");
                return true;
            }
        }
    }
    return false;
}
