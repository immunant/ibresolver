extern "C" {
#include "qemu/qemu-plugin.h"
}

#include "callsites.h"
#include <iostream>
#include <vector>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

using namespace std;
typedef struct addr_range {
  uint64_t start_addr;
  uint64_t end_addr;
} addr_range;

std::vector<addr_range> indirect_blocks;
static uint64_t indirect_callsite = 0;

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
  uint64_t start_vaddr = *(uint64_t *)start;
  if (indirect_callsite) {
    std::cout << "indirect jump at " << indirect_callsite << " went to "
              << start_vaddr << std::endl;
    indirect_callsite = 0;
  }
}

static void indirect_block_exec_handler(unsigned int vcpu_idx, void *block) {
  addr_range *block_addr = (addr_range *)block;
  if (indirect_callsite) {
    std::cout << "indirect jump at " << indirect_callsite << " went to "
              << block_addr->start_addr << std::endl;
  }
  indirect_callsite = block_addr->end_addr;
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
  size_t num_callsites = sizeof(callsites) / sizeof(callsites[0]);
  for (size_t i = 0; i < num_callsites; i++) {
    if (last_insn.start_addr == callsites[i]) {
      has_indirect = true;
      addr_range block_addr = {
          .start_addr = start_vaddr,
          .end_addr = last_insn.start_addr,
      };
      qemu_plugin_register_vcpu_tb_exec_cb(tb, indirect_block_exec_handler,
                                           QEMU_PLUGIN_CB_NO_REGS, &block_addr);
      break;
    }
  }
  if (!has_indirect) {
    qemu_plugin_register_vcpu_tb_exec_cb(tb, block_exec_handler,
                                         QEMU_PLUGIN_CB_NO_REGS, &start_vaddr);
  }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                               int argc, char **argv) {
  // if (argc != 1) {
  //    printf("uh oh\n");
  //    return -1;
  //}
  std::cout << "loading indirect jmp/call resolver plugin" << std::endl;
  qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);
  return 0;
}
