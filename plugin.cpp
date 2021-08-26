extern "C" {
#include "qemu/qemu-plugin.h"
}

#include <elf.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

using namespace std;

// List of callsites obtained from input file
static vector<uint64_t> callsites;

// Address of previous callsite if it was an indirect jump/call
static optional<uint64_t> branch_addr = {};

static ofstream outfile;

static string binary_name;
static optional<uint64_t> binary_bias = {};
static bool dynamically_linked = false;

typedef struct segment {
    string so_name;
    uint64_t load_bias;
    uint64_t image_offset;
} segment;

static optional<uint64_t> file_load_bias(const char *filename, const string_view segment) {
    char name[100];
    uint64_t load_bias;
    sscanf(segment.data(), "%lx-%*lx %*c%*c%*c%*c %*lx %*lx:%*lx %*lu %s", &load_bias, name);
    if (!strcmp(name, filename)) {
        return load_bias;
    }
    return {};
}

static optional<segment> addr_in_segment(uint64_t dst_vaddr, const string_view segment) {
    char name[100];
    uint64_t from, to, offset;
    sscanf(segment.data(), "%lx-%lx %*c%*c%*c%*c %lx %*lx:%*lx %*lu %s", &from, &to, &offset, name);
    if ((from <= dst_vaddr) && (dst_vaddr <= to)) {
        struct segment seg = {
            .so_name = string(name),
            .load_bias = from,
            .image_offset = offset,
        };
        return seg;
    }
    return {};
}

// Write the destination of an indirect jump/call to the output file
static void mark_indirect_branch(uint64_t callsite, uint64_t dst_vaddr) {
    ifstream maps("/proc/self/maps");
    string line;
    while (getline(maps, line)) {
        optional<segment> seg = addr_in_segment(dst_vaddr, line);
        if (seg.has_value()) {
            uint64_t dst_offset = dst_vaddr;
            if (dynamically_linked) {
                dst_offset -= seg->load_bias - seg->image_offset;
            }
            outfile << "0x" << hex << callsite << ",0x" << hex << dst_offset << "," << seg->so_name
                    << endl;
            return;
        }
    }
}

// Callback for insn at the start of a block. Takes an absolute vaddr.
static void branch_taken(unsigned int vcpu_idx, void *dst_vaddr) {
    if (branch_addr.has_value()) {
        mark_indirect_branch(branch_addr.value(), (uint64_t)dst_vaddr);
        branch_addr = {};
    }
}

// Callback for insn following an indirect branch
static void branch_skipped(unsigned int vcpu_idx, void *userdata) { branch_addr = {}; }

// Callback for indirect branch insn. Takes a vaddr relative to the binary bias.
static void indirect_branch_exec(unsigned int vcpu_idx, void *callsite_addr) {
    uint64_t insn_addr = (uint64_t)callsite_addr;
    branch_addr = insn_addr;
}

// Callback for indirect branch at the start of a block. Takes an absolute vaddr.
static void indirect_branch_at_start(unsigned int vcpu_idx, void *callsite_addr) {
    branch_taken(vcpu_idx, callsite_addr);

    uint64_t vaddr_offset = (uint64_t)callsite_addr - binary_bias.value();
    indirect_branch_exec(vcpu_idx, (void *)vaddr_offset);
}

static uint64_t tb_last_insn_vaddr(struct qemu_plugin_tb *tb) {
    uint64_t last_idx = qemu_plugin_tb_n_insns(tb) - 1;
    struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, last_idx);
    return qemu_plugin_insn_vaddr(insn);
}

// Register a callback for each time a block is executed
static void block_trans_handler(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
    // The binary is loaded after the plugin is installed so we resort to doing a post-install initialization here.
    if (!binary_bias.has_value()) {
        // Check if ELF is dynamically linked
        ifstream binary(binary_name, ifstream::binary);
        // E_TYPE is at the same offset in both 32 and 64-bit ELFs
        binary.seekg(EI_NIDENT);
        uint16_t e_type;
        binary.read(reinterpret_cast<char *>(&e_type), sizeof(e_type));
        dynamically_linked = e_type == ET_DYN;

        if (dynamically_linked) {
            ifstream maps("/proc/self/maps");
            string line;
            while (getline(maps, line)) {
                optional<uint64_t> load_bias = file_load_bias(binary_name.data(), line);
                if (load_bias.has_value()) {
                    binary_bias = load_bias.value();
                    break;
                }
            }
        } else {
            binary_bias = 0;
        }
    }

    uint64_t start_vaddr = qemu_plugin_tb_vaddr(tb);
    uint64_t last_insn = tb_last_insn_vaddr(tb);
    size_t n_insns = qemu_plugin_tb_n_insns(tb);

    for (size_t i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t insn_addr = qemu_plugin_insn_vaddr(insn) - binary_bias.value();
        bool insn_is_branch = binary_search(callsites.begin(), callsites.end(), insn_addr);
        // The callback for the first instruction in a block should mark the indirect branch
        // destination if one was taken
        if (i == 0) {
            if (!insn_is_branch) {
                // Since the dest vaddr can be in any .so, we don't subtract the binary bias from
                // the callback arg
                qemu_plugin_register_vcpu_insn_exec_cb(insn, branch_taken, QEMU_PLUGIN_CB_NO_REGS,
                                                       (void *)start_vaddr);
            } else {
                // If the first branch is also an indirect branch, the callback must mark the
                // destination and update `branch_addr` Don't subtract the binary bias from the
                // callback arg like in `branch_taken`
                qemu_plugin_register_vcpu_insn_exec_cb(insn, indirect_branch_at_start,
                                                       QEMU_PLUGIN_CB_NO_REGS, (void *)start_vaddr);
                // The following insn should clear `branch_addr` like below
                if (n_insns > 1) {
                    struct qemu_plugin_insn *next_insn = qemu_plugin_tb_get_insn(tb, 1);
                    qemu_plugin_register_vcpu_insn_exec_cb(next_insn, branch_skipped,
                                                           QEMU_PLUGIN_CB_NO_REGS, NULL);
                }
            }
        } else {
            if (insn_is_branch) {
                qemu_plugin_register_vcpu_insn_exec_cb(insn, indirect_branch_exec,
                                                       QEMU_PLUGIN_CB_NO_REGS, (void *)insn_addr);
                if (i + 1 < n_insns) {
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
    if (argc < 3) {
        cout << "Usage: /path/to/qemu \\" << endl;
        cout << "\t-plugin /path/to/libibresolver.so,arg=\"callsites.txt\",arg=\"output.csv\",arg=\"/absolute/path/to/$BINARY\"" << endl;
        cout << "\t$BINARY" << endl;
        return -1;
    }

    ifstream input(argv[0]);
    outfile = ofstream(argv[1]);
    // TODO: It's surprisingly difficult to get the binary name from a QEMU plugin so we currently pass the name as a plugin arg.
    // A better approach would be to go from vaddr to name using /proc/self/maps, but this isn't straightforward since execution start in the ELF interpreter so we can't do it in the post-install initialization.
    // The way to go is probably to remove the `callsites` input and support branches originating from anywhere by checking for branches in the translate block callback then we'd just need the name of the ELF corresponding to the block being translated.
    binary_name = string(argv[2]);

    if (input.fail()) {
        cout << "Could not open file " << argv[0] << endl;
        return -2;
    }

    uint64_t addr;
    while (input >> hex >> addr) {
        callsites.push_back(addr);
    }
    sort(callsites.begin(), callsites.end());
    cout << "Loaded input file with " << callsites.size() << " indirect callsites" << endl;
    outfile << "callsite,destination offset,destination image" << endl;
    // Register a callback for each time a block is translated
    qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);

    return 0;
}
