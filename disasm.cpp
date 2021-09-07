#include "disasm.h"
#include "binaryninjacore.h"
#include "binaryninjaapi.h"

using namespace BinaryNinja;

static BNArchitecture *arch = NULL;

extern "C" bool init_backend(const char *arch_name) {
    BNSetBundledPluginDirectory(BINJA_PLUGIN_DIR);
    BNInitPlugins(true);
    arch = BNGetArchitectureByName(arch_name);
    return arch;
}

extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size) {
    // These is a non-extensive workaround for indirect branches that binja doesn't report
    // TODO: Figure out if this is a bug in binja and file an issue
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
    // Handles blx rn
    if (insn_size == 4) {
        uint32_t w = *(uint32_t *)insn_data;
        // Set variable bitfields in the instruction
        w |= 0xf000000f;
        if (w == 0xf12fff3f) {
            return true;
        }
    }
    BNInstructionInfo info;
    BNGetInstructionInfo(arch, insn_data, 0 /* addr */, insn_size, &info);
    if (info.branchCount) {
        BNBranchType br = info.branchType[0];
        return (br == BNBranchType::UnresolvedBranch) || (br == BNBranchType::IndirectBranch);
    }
    return false;
}
