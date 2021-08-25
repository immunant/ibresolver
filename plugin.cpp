extern "C" {
#include "qemu/qemu-plugin.h"
}

#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

// Syscall numbers taken from
// https://chromium.googlesource.com/chromiumos/docs/+/HEAD/constants/syscalls.md
#define ARM32_MMAP2 192
#define ARM32_OPENAT 322
#define X86_64_MMAP 9
#define X86_64_OPENAT 257

static int64_t MMAP = 0;
static int64_t OPENAT = 0;

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

using namespace std;

typedef struct addr_range {
    uint64_t start_addr;
    uint64_t end_addr;
} addr_range;

typedef struct shared_obj {
    const char *filename;
    int fd;
} shared_obj;

typedef struct mapped_section {
    uint64_t load_bias;
    uint64_t image_offset;
    const char *so_name;
} mapped_section;

static vector<shared_obj> shared_objects;
static vector<mapped_section> sections;

static size_t indirect_tb_idx = 0;
// Ranges of addresses for each block ending in an indirect jump/call
static vector<addr_range> indirect_blocks;

// List of callsites obtained from input file
static vector<uint64_t> callsites;

// Address of previous callsite if it was an indirect jump/call
static optional<uint64_t> indirect_taken = {};

static ofstream outfile;

// Get the addresses of the first and last bytes of the last instruction in a
// block
static uint64_t tb_last_insn_vaddr(struct qemu_plugin_tb *tb) {
    uint64_t last_idx = qemu_plugin_tb_n_insns(tb) - 1;
    struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, last_idx);
    return qemu_plugin_insn_vaddr(insn);
}

static uint64_t elf_image_bias(uint64_t vaddr) {
    vector<uint64_t> potential_load_biases;
    if (get_load_bias() <= vaddr) {
        potential_load_biases.push_back(get_load_bias());
    }
    if (get_interp_load_bias() <= vaddr) {
        potential_load_biases.push_back(get_interp_load_bias());
    }
    for (auto &sec : sections) {
        if (sec.load_bias <= vaddr) {
            potential_load_biases.push_back(sec.load_bias);
        }
    }
    // TODO: Can there ever be no potential_load_biases?
    return *max_element(potential_load_biases.begin(), potential_load_biases.end());
}

static size_t find_section(uint64_t bias) {
    for (size_t i = 0; i < sections.size(); i++) {
        if (sections[i].load_bias == bias) {
            return i;
        }
    }
    return SIZE_MAX;
}

static bool dynamically_linked() { return get_interp_load_bias() != 0; }

// Write the destination of an indirect jump/call to the output file
static void mark_indirect_branch(uint64_t callsite, uint64_t dst) {
    uint64_t dst_image_bias = elf_image_bias(dst);
    uint64_t dst_image_offset;
    const char *so_name;
    if (dst_image_bias == get_load_bias()) {
        dst_image_offset = 0;
        so_name = "binary";
    } else if (dst_image_bias == get_interp_load_bias()) {
        dst_image_offset = 0;
        so_name = "interpreter";
    } else {
        size_t idx = find_section(dst_image_bias);
        mapped_section sec = sections[idx];
        dst_image_offset = sec.image_offset;
        so_name = sec.so_name;
    }

    if (dynamically_linked()) {
        dst -= dst_image_bias - dst_image_offset;
        callsite -= get_load_bias();
    }

    outfile << "0x" << hex << callsite << ",0x" << hex << dst << "," << so_name << endl;
}

// The default callback for when a block is executed
static void block_exec_handler(unsigned int vcpu_idx, void *start) {
    uint64_t start_vaddr = (uint64_t)start;
    if (indirect_taken.has_value()) {
        mark_indirect_branch(indirect_taken.value(), start_vaddr);
        indirect_taken = {};
    }
}

// Callback for executing blocks ending in an indirect jump/call
static void indirect_block_exec_handler(unsigned int vcpu_idx, void *tb_idx) {
    addr_range block_addr = indirect_blocks[(size_t)tb_idx];

    // Check if the previous block ended in an indirect jump/call
    if (indirect_taken.has_value()) {
        mark_indirect_branch(indirect_taken.value(), block_addr.start_addr);
    }

    indirect_taken = block_addr.end_addr;
}

// Register a callback for each time a block is executed
static void block_trans_handler(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
    static uint64_t start_vaddr;
    start_vaddr = qemu_plugin_tb_vaddr(tb);
    uint64_t last_insn = tb_last_insn_vaddr(tb);
    uint64_t bias = 0;

    // If an interpreter was loaded, add the binary bias to the input callsites
    if (dynamically_linked()) {
        bias = get_load_bias();
    }

    for (uint64_t &addr : callsites) {
        if (last_insn == (addr + bias)) {
            indirect_blocks.push_back({
                .start_addr = start_vaddr,
                .end_addr = last_insn,
            });
            qemu_plugin_register_vcpu_tb_exec_cb(tb, indirect_block_exec_handler,
                                                 QEMU_PLUGIN_CB_NO_REGS, (void *)indirect_tb_idx++);
            return;
        }
    }
    qemu_plugin_register_vcpu_tb_exec_cb(tb, block_exec_handler, QEMU_PLUGIN_CB_NO_REGS,
                                         (void *)start_vaddr);
}

static void syscall_handler(qemu_plugin_id_t id, unsigned int vcpu_index, int64_t num, uint64_t a1,
                            uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                            uint64_t a7, uint64_t a8) {
    if (num == MMAP) {
        // Map a shared object file name to a `mapped_section` when entering an
        // mmap syscall
        int fd = (int)a5;
        uint64_t load_bias = a1;
        uint64_t image_offset = a6;

        auto matching_fd = [&](shared_obj so) { return so.fd == fd; };
        // file descriptors can be reused so search for the /last/ ocurrence of
        // an opened file with a file descriptor matching the mmap call
        auto so = find_if(shared_objects.rbegin(), shared_objects.rend(), matching_fd);
        if (so != shared_objects.rend()) {
            mapped_section sec = {
                .load_bias = load_bias,
                .image_offset = image_offset,
                .so_name = so->filename,
            };
            sections.push_back(sec);
        }
    } else if (num == OPENAT) {
        // TODO: Is the open syscall also used to open shared objects?
        // Store the file name passed to the openat syscall
        shared_obj lib = {
            .filename = (char *)a2,
            .fd = -1,
        };
        shared_objects.push_back(lib);
    }
}

static void syscall_ret_handler(qemu_plugin_id_t id, unsigned int vcpu_idx, int64_t num,
                                int64_t ret) {
    // If the openat syscall returned a valid file descriptor
    if ((num == OPENAT) && (ret != -1)) {
        // Store the file descriptor returned by the syscall
        shared_objects.back().fd = ret;
    }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc,
                               char **argv) {
    if (argc < 2) {
        cout << "Usage: /path/to/qemu \\\n";
        cout << "\t-plugin "
                "/path/to/"
                "libibresolver.so,arg=\"callsites.txt\",arg=\"output.csv\" \\\n";
        cout << "\t$BINARY" << endl;
        return -1;
    }

    fstream input(argv[0]);
    outfile = ofstream(argv[1]);
    if (input.fail()) {
        cout << "Could not open file " << argv[0] << endl;
        return -2;
    }

    if (!strcmp(info->target_name, "x86_64")) {
        MMAP = X86_64_MMAP;
        OPENAT = X86_64_OPENAT;
    } else if (!strcmp(info->target_name, "arm")) {
        MMAP = ARM32_MMAP2;
        OPENAT = ARM32_OPENAT;
    } else {
        cout << "Unsupported architecture " << info->target_name << endl;
        return -3;
    }

    uint64_t addr;
    while (input >> hex >> addr) {
        callsites.push_back(addr);
    }
    cout << "Loaded input file with " << callsites.size() << " indirect callsites" << endl;
    outfile << "callsite,destination offset,destination image" << endl;
    // Register a callback for each time a block is translated
    qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);

    // Register callbacks for entering and returning from syscalls
    // This is used to determine the load biases and image offsets for
    // dynamically linked shared objects
    qemu_plugin_register_vcpu_syscall_cb(id, syscall_handler);
    qemu_plugin_register_vcpu_syscall_ret_cb(id, syscall_ret_handler);
    return 0;
}
