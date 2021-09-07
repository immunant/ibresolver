extern "C" {
#include "qemu/qemu-plugin.h"
// TODO: Move this into the QEMU plugin API
uintptr_t guest_base;
}

#include <string.h>

#include <fstream>
#include <iostream>
#include <optional>

#include "disasm.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

using namespace std;

// Address of previous callsite if it was an indirect jump/call
static optional<uint64_t> branch_addr = {};

static ofstream outfile;

typedef struct image_offset {
    // An offset into a memory-mapped ELF image
    uint64_t offset;
    // The position of the image name in its /proc/self/maps entry
    size_t image_name_pos;
} image_offset;

// Returns an `image_offset` for the input vaddr if it's in the memory-mapped region defined by the
// /proc/self/maps entry.
static optional<image_offset> get_offset(const string_view maps_entry, uint64_t emulator_vaddr) {
    uint32_t name_pos;
    uint64_t from, to, offset;
    uint64_t real_vaddr = emulator_vaddr + guest_base;
    sscanf(maps_entry.data(), "%lx-%lx %*c%*c%*c%*c %lx %*lx:%*lx %*lu %n", &from, &to, &offset,
           &name_pos);
    if ((from <= real_vaddr) && (real_vaddr <= to)) {
        struct image_offset offset = {
            .offset = real_vaddr - from,
            .image_name_pos = name_pos,
        };
        return offset;
    }
    return {};
}

// Write the destination of an indirect jump/call to the output file
static void mark_indirect_branch(uint64_t callsite_vaddr, uint64_t dst_vaddr) {
    ifstream maps("/proc/self/maps");
    string line;
    optional<image_offset> callsite = {};
    optional<image_offset> dst = {};
    string callsite_image = "";
    string dst_image = "";
    // For each entry in /proc/self/maps
    while (getline(maps, line)) {
        if (!callsite.has_value()) {
            callsite = get_offset(line, callsite_vaddr);
            if (callsite.has_value()) {
                // Copy name since `line` gets reused in this loop
                char *image_name = line.data() + callsite->image_name_pos;
                callsite_image = string(image_name);
            }
        }
        if (!dst.has_value()) {
            dst = get_offset(line, dst_vaddr);
            if (dst.has_value()) {
                // Copy name since `line` gets reused in this loop
                char *image_name = line.data() + dst->image_name_pos;
                dst_image = string(image_name);
            }
        }
        // Skip the remaining entries after finding the callsite and destination
        if (callsite.has_value() && dst.has_value()) {
            break;
        }
    }
    if (!callsite.has_value()) {
        cout << "ERROR: Unable to find callsite address in /proc/self/maps" << endl;
    }
    if (!dst.has_value()) {
        cout << "ERROR: Unable to find destination address in /proc/self/maps" << endl;
    }
    outfile << "0x" << hex << callsite->offset << "," << callsite_image << ",0x" << hex << dst->offset << "," << dst_image << endl;
    return;
};

// Callback for insn at the start of a block
static void branch_taken(unsigned int vcpu_idx, void *dst_vaddr) {
    if (branch_addr.has_value()) {
        mark_indirect_branch(branch_addr.value(), (uint64_t)dst_vaddr);
        branch_addr = {};
    }
}

// Callback for insn following an indirect branch
static void branch_skipped(unsigned int vcpu_idx, void *userdata) { branch_addr = {}; }

// Callback for indirect branch insn
static void indirect_branch_exec(unsigned int vcpu_idx, void *callsite_addr) {
    branch_addr = (uint64_t)callsite_addr;
}

// Callback for indirect branch which may also be the destination of another branch
static void indirect_branch_at_start(unsigned int vcpu_idx, void *callsite_addr) {
    branch_taken(vcpu_idx, callsite_addr);
    indirect_branch_exec(vcpu_idx, callsite_addr);
}

// Register a callback for each time a block is executed
static void block_trans_handler(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
    uint64_t start_vaddr = qemu_plugin_tb_vaddr(tb);
    size_t num_insns = qemu_plugin_tb_n_insns(tb);

    for (size_t i = 0; i < num_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t insn_addr = qemu_plugin_insn_vaddr(insn);

        uint8_t *insn_data = (uint8_t *)qemu_plugin_insn_data(insn);
        size_t insn_size = qemu_plugin_insn_size(insn);

        bool insn_is_branch = is_indirect_branch(insn_data, insn_size);
        // The callback for the first instruction in a block should mark the indirect branch
        // destination if one was taken
        if (i == 0) {
            if (!insn_is_branch) {
                qemu_plugin_register_vcpu_insn_exec_cb(insn, branch_taken, QEMU_PLUGIN_CB_NO_REGS,
                                                       (void *)start_vaddr);
            } else {
                // If the first branch is also an indirect branch, the callback must mark the
                // destination and update `branch_addr`
                qemu_plugin_register_vcpu_insn_exec_cb(insn, indirect_branch_at_start,
                                                       QEMU_PLUGIN_CB_NO_REGS, (void *)start_vaddr);
                // In this case the second insn should clear `branch_addr` like below
                if (num_insns > 1) {
                    struct qemu_plugin_insn *next_insn = qemu_plugin_tb_get_insn(tb, 1);
                    qemu_plugin_register_vcpu_insn_exec_cb(next_insn, branch_skipped,
                                                           QEMU_PLUGIN_CB_NO_REGS, NULL);
                }
            }
        } else {
            if (insn_is_branch) {
                qemu_plugin_register_vcpu_insn_exec_cb(insn, indirect_branch_exec,
                                                       QEMU_PLUGIN_CB_NO_REGS, (void *)insn_addr);
                if (i + 1 < num_insns) {
                    struct qemu_plugin_insn *next_insn = qemu_plugin_tb_get_insn(tb, i + 1);
                    qemu_plugin_register_vcpu_insn_exec_cb(next_insn, branch_skipped,
                                                           QEMU_PLUGIN_CB_NO_REGS, NULL);
                }
            }
        }
    }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc,
                               char **argv) {
    if (argc < 1) {
        cout << "Usage: /path/to/qemu \\" << endl;
        cout << "\t-plugin /path/to/libibresolver.so,arg=\"output.csv\" \\" << endl;
        cout << "\t$BINARY" << endl;
        return -1;
    }

    outfile = ofstream(argv[0]);
    if (outfile.fail()) {
        cout << "Could not open file " << argv[0] << endl;
        return -2;
    }

    const char *arch;
    if (!strcmp(info->target_name, "arm")) {
        arch = "armv7";
    } else if (!strcmp(info->target_name, "x86_64")) {
        arch = info->target_name;
    } else {
        cout << "Unsupported qemu architecture" << endl;
        return -3;
    }
    if (!init_backend(arch)) {
        cout << "Could not initialize disassembly backend for " << arch << endl;
        return -4;
    }

    outfile << "callsite offset,callsite image,destination offset,destination image" << endl;
    // Register a callback for each time a block is translated
    qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);

    return 0;
}
