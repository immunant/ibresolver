#include <cstring>
#include "disasm.h"
#include "binaryninjacore.h"
#include "binaryninjaapi.h"

using namespace BinaryNinja;

static BNArchitecture *arch = NULL;

extern "C" bool init_backend(const char *arch_name) {
    const char *binja_arch;
    if (!strcmp(arch_name, "arm")) {
        binja_arch = "armv7";
    } else if (!strcmp(arch_name, "x86_64")) {
        binja_arch = arch_name;
    } else {
        return false;
    }
    BNSetBundledPluginDirectory(BINJA_PLUGIN_DIR);
    BNInitPlugins(true);
    arch = BNGetArchitectureByName(binja_arch);
    return arch;
}

extern "C" bool is_indirect_branch(uint8_t *insn_data, size_t insn_size) {
    // This is a non-extensive workaround for indirect branches that binja doesn't report
    // TODO: Figure out if this is a bug in binja and file an issue
    if (is_indirect_branch_impl(insn_data, insn_size)) {
        return true;
    }
    BNInstructionInfo info;
    BNGetInstructionInfo(arch, insn_data, 0 /* addr */, insn_size, &info);
    if (info.branchCount) {
        BNBranchType br = info.branchType[0];
        return (br == BNBranchType::UnresolvedBranch) || (br == BNBranchType::IndirectBranch);
    }
    return false;
}
