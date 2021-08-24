extern "C" {
#include "qemu/qemu-plugin.h"
}

#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

using namespace std;

typedef struct addr_range {
  uint64_t start_addr;
  uint64_t end_addr;
} addr_range;

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
  uint64_t bias;
  uint64_t bin_bias = get_load_bias();
  uint64_t interp_bias = get_interp_load_bias();
  if ((vaddr >= bin_bias) && (vaddr >= interp_bias)) {
    bias = max(bin_bias, interp_bias);
  } else {
    bias = min(bin_bias, interp_bias);
  }
  return bias;
}

// Write the destination of an indirect jump/call to the output file
static void mark_indirect_branch(uint64_t callsite, uint64_t dst) {
  uint64_t dst_bias = elf_image_bias(dst);
  uint64_t bin_bias = get_load_bias();
  //const char *image_name;
  if (dst_bias != bin_bias) {
      return;
  };
  //  image_name = "interpreter";
  //} else {
  //  image_name = "binary";
  //}
  outfile << "0x" << hex << callsite - bin_bias << ",0x" << hex << dst - dst_bias << endl;
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
static void block_trans_handler(qemu_plugin_id_t id,
                                struct qemu_plugin_tb *tb) {
  static uint64_t start_vaddr;
  start_vaddr = qemu_plugin_tb_vaddr(tb);
  uint64_t last_insn = tb_last_insn_vaddr(tb);
  uint64_t bias = 0;

  // If an interpreter was loaded, add the binary bias to the input callsites
  if (get_interp_load_bias()) {
    bias = get_load_bias();
  }

  for (uint64_t &addr : callsites) {
    if (last_insn == (addr + bias)) {
      indirect_blocks.push_back({
          .start_addr = start_vaddr,
          .end_addr = last_insn,
      });
      qemu_plugin_register_vcpu_tb_exec_cb(tb, indirect_block_exec_handler,
                                           QEMU_PLUGIN_CB_NO_REGS,
                                           (void *)indirect_tb_idx++);
      return;
    }
  }
  qemu_plugin_register_vcpu_tb_exec_cb(
      tb, block_exec_handler, QEMU_PLUGIN_CB_NO_REGS, (void *)start_vaddr);
}

static void syscall_handler(qemu_plugin_id_t id, unsigned int vcpu_index,
                                 int64_t num, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5,
                                 uint64_t a6, uint64_t a7, uint64_t a8) {
    // TODO: Where are linux syscall numbers defined? 
    if (num == 9) {
        // mmap
        cout << "called mmap(0x" << hex << a1 << ", " << hex << a2 << ")" << endl;
        int fd = a5;
        off_t offset = a6;
        cout << "fd: " << fd << ", offset: 0x" << hex << offset << endl;
    } else if (num == 2) {
        // open
        const char *filename = (char *)a1;
        cout << "called open " << filename << endl;
    }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                               int argc, char **argv) {
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
  uint64_t addr;
  while (input >> hex >> addr) {
    callsites.push_back(addr);
  }
  cout << "Loaded input file with " << callsites.size() << " indirect callsites"
       << endl;
  outfile << "callsite,destination" << endl;
  // Register a callback for each time a block is translated
  qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);
  qemu_plugin_register_vcpu_syscall_cb(id, syscall_handler);
  return 0;
}
