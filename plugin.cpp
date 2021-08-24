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
static vector<uint64_t> callsites;
static vector<addr_range> indirect_blocks;
static optional<uint64_t> indirect_taken = {};

static addr_range tb_last_insn(struct qemu_plugin_tb *tb) {
  uint64_t last_idx = qemu_plugin_tb_n_insns(tb) - 1;
  struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, last_idx);
  uint64_t start = qemu_plugin_insn_vaddr(insn);
  uint64_t end = start + qemu_plugin_insn_size(insn) - 1;
  addr_range last_insn = {
      .start_addr = start,
      .end_addr = end,
  };
  return last_insn;
}

static void mark_indirect_call(uint64_t callsite, uint64_t dst) {
  cout << hex << callsite << "," << hex << dst << endl;
}

static void block_exec_handler(unsigned int vcpu_idx, void *start) {
  uint64_t start_vaddr = (uint64_t)start;
  if (indirect_taken.has_value()) {
    mark_indirect_call(indirect_taken.value(), start_vaddr);
    indirect_taken = {};
  }
}

static void indirect_block_exec_handler(unsigned int vcpu_idx, void *tb_idx) {
  addr_range block_addr = indirect_blocks[(size_t)tb_idx];
  if (indirect_taken.has_value()) {
    mark_indirect_call(indirect_taken.value(), block_addr.start_addr);
  }
  indirect_taken = block_addr.end_addr;
}

/// Checks if any of the input indirect jumps/call sites are the final
/// instruction in the block being translated and assigns a block handler
/// accordingly.
static void block_trans_handler(qemu_plugin_id_t id,
                                struct qemu_plugin_tb *tb) {
  static uint64_t start_vaddr;
  start_vaddr = qemu_plugin_tb_vaddr(tb);
  addr_range last_insn = tb_last_insn(tb);
  uint64_t end_vaddr = last_insn.end_addr;

  for (uint64_t &addr : callsites) {
    if (last_insn.start_addr == addr) {
      indirect_blocks.push_back({
          .start_addr = start_vaddr,
          .end_addr = last_insn.start_addr,
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

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                               int argc, char **argv) {
  if (argc != 1) {
    cout << "No indirect jump/call sites provided" << endl;
    return -1;
  }

  fstream input(argv[0]);
  if (input.fail()) {
    cout << "Could not open file " << argv[0] << endl;
    return -2;
  }
  uint64_t addr;
  while (input >> hex >> addr) {
    callsites.push_back(addr);
  }
  cout << "Loaded input file with " << callsites.size() << " indirect callsites" << endl;
  cout << "callsite,destination" << endl;
  qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);
  return 0;
}
