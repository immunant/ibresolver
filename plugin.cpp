extern "C" {
#include "qemu/qemu-plugin.h"
}

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
            uint64_t dst_offset = dst_vaddr - seg->load_bias + seg->image_offset;
            outfile << "0x" << hex << callsite << ",0x" << hex << dst_offset << "," << seg->so_name << endl;
            return;
        }
    }
}

static void branch_taken(unsigned int vcpu_idx, void *dst_vaddr) {
    if (branch_addr.has_value()) {
        mark_indirect_branch(branch_addr.value(), (uint64_t)dst_vaddr);
        branch_addr = {};
    }
}

static void branch_skipped(unsigned int vcpu_idx, void *userdata) {
    branch_addr = {};
}

static void indirect_branch_exec(unsigned int vcpu_idx, void *callsite_addr) {
    uint64_t insn_addr = (uint64_t)callsite_addr;
    branch_addr = insn_addr;
}

static uint64_t tb_last_insn_vaddr(struct qemu_plugin_tb *tb) {
    uint64_t last_idx = qemu_plugin_tb_n_insns(tb) - 1;
    struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, last_idx);
    return qemu_plugin_insn_vaddr(insn);
}

#include <unistd.h>
// Register a callback for each time a block is executed
static void block_trans_handler(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
    // The binary is loaded after the plugin is installed so we resort to getting the binary bias here
    if (!binary_bias.has_value()) {
        ifstream maps("/proc/self/maps");
        string line;
        while (getline(maps, line)) {
            optional<uint64_t> load_bias = file_load_bias(binary_name.data(), line);
            if (load_bias.has_value()) {
                binary_bias = load_bias.value();
                break;
            }
        }
    }
    cout << "pid is " << getpid() << endl;
    while (1) {}

    uint64_t start_vaddr = qemu_plugin_tb_vaddr(tb);
    uint64_t last_insn = tb_last_insn_vaddr(tb);
    size_t n_insns = qemu_plugin_tb_n_insns(tb);

    for (size_t i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t insn_addr = qemu_plugin_insn_vaddr(insn) - binary_bias.value();
        bool insn_is_branch = binary_search(callsites.begin(), callsites.end(), insn_addr);
        if (insn_is_branch) {
            qemu_plugin_register_vcpu_insn_exec_cb(insn, indirect_branch_exec, QEMU_PLUGIN_CB_NO_REGS, (void *)insn_addr);
            if (i + 1 < n_insns) {
                struct qemu_plugin_insn *next_insn = qemu_plugin_tb_get_insn(tb, i + 1);
                qemu_plugin_register_vcpu_insn_exec_cb(next_insn, branch_skipped, QEMU_PLUGIN_CB_NO_REGS, NULL);
            }
        }
    }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc,
                               char **argv) {
    if (argc < 3) {
        cout << "Usage: /path/to/qemu \\\n";
        cout << "\t-plugin "
                "/path/to/"
                "libibresolver.so,arg=\"callsites.txt\",arg=\"output.csv\" \\\n";
        cout << "\t$BINARY" << endl;
        return -1;
    }

    ifstream input(argv[0]);
    outfile = ofstream(argv[1]);
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
