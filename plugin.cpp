extern "C" {
#include "qemu/qemu-plugin.h"
}

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

using namespace std;

typedef struct addr_range {
  uint64_t start_addr;
  uint64_t end_addr;
} addr_range;

static vector<uint64_t> callsites;
static vector<addr_range> indirect_blocks;
static optional<uint64_t> from_indirect = {};

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

static void block_exec_handler(unsigned int vcpu_idx, void *start) {
  uint64_t start_vaddr = (uint64_t)start;
  if (from_indirect.has_value()) {
    cout << "indirect jump at " << from_indirect.value() << " went to "
              << start_vaddr << endl;
    from_indirect = {};
  }
}

static void indirect_block_exec_handler(unsigned int vcpu_idx, void *block) {
  addr_range *block_addr = (addr_range *)block;
  if (from_indirect.has_value()) {
    cout << "indirect jump at " << from_indirect.value() << " went to "
              << block_addr->start_addr << endl;
  }
  from_indirect = block_addr->end_addr;
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

  bool has_indirect = false;
  for (auto& addr : callsites) {
      if (last_insn.start_addr == addr) {
          has_indirect = true;
          indirect_blocks.push_back({
              .start_addr = start_vaddr,
              .end_addr = last_insn.start_addr,
          });
          qemu_plugin_register_vcpu_tb_exec_cb(tb, indirect_block_exec_handler, QEMU_PLUGIN_CB_NO_REGS, &indirect_blocks.back());
          break;
      }
  }
  if (!has_indirect) {
      qemu_plugin_register_vcpu_tb_exec_cb(tb, block_exec_handler, QEMU_PLUGIN_CB_NO_REGS, (void *)start_vaddr);
  }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                               int argc, char **argv) {
  if (argc != 1) {
      cout << "No indirect jump/call sites provided" << endl;
      return -1;
  }

  cout << "Loading indirect jmp/call resolver plugin" << endl;
  fstream input(argv[0]);
  if (input.fail()) {
      cout << "Could not open file " << argv[0] << endl;
      return -2;
  }
  uint64_t addr;
  while (input >> hex >> addr) {
      callsites.push_back(addr);
  }
  cout << "Loaded " << callsites.size() << " indirect callsites" << endl;
  qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);
  return 0;
}
