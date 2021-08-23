#include "qemu/qemu-plugin.h"
#include <stdio.h>
#include <assert.h>
#include "callsites.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct last_insn {
    uint64_t start_addr;
    uint64_t end_addr;
} last_insn;

static void tb_last_insn(struct qemu_plugin_tb* tb, last_insn *last) {
    uint64_t last_idx = qemu_plugin_tb_n_insns(tb) - 1;
    struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, last_idx);
    uint64_t start = qemu_plugin_insn_vaddr(insn);
    uint64_t end = start + qemu_plugin_insn_size(insn);
    last->start_addr = start;
    last->end_addr = end;
}

static void block_exec_handler(unsigned int vcpu_idx, void* userdata) {
    printf("executed a block\n");
}

static void indirect_block_exec_handler(unsigned int vcpu_idx, void* userdata) {
    printf("executed a block ending in an indirect jump\n");
}

static void block_trans_handler(qemu_plugin_id_t id, struct qemu_plugin_tb* tb) {
    uint64_t start_vaddr = qemu_plugin_tb_vaddr(tb);
    last_insn last;
    tb_last_insn(tb, &last);
    uint64_t end_vaddr = last.end_addr;

    bool has_indirect = false;
    size_t num_callsites = sizeof(callsites) / sizeof(callsites[0]);
    for (size_t i = 0; i < num_callsites; i++) {
        if ((start_vaddr <= callsites[i]) && (callsites[i] <= end_vaddr)) {
            assert(last.start_addr == callsites[i]);
            printf("translated block from vaddrs %lx to %lx containing %lx\n",  start_vaddr, end_vaddr, callsites[i]);
            has_indirect = true;
            qemu_plugin_register_vcpu_tb_exec_cb(tb, indirect_block_exec_handler, QEMU_PLUGIN_CB_NO_REGS, NULL);
            break;
        }
    }
    if (!has_indirect) {
        qemu_plugin_register_vcpu_tb_exec_cb(tb, block_exec_handler, QEMU_PLUGIN_CB_NO_REGS, NULL);
    }
}

extern int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
    //if (argc != 1) {
    //    printf("uh oh\n");
    //    return -1;
    //}
    printf("loading iresolver plugin\n");
    printf("%lu\n", sizeof(callsites) / 8);
    qemu_plugin_register_vcpu_tb_trans_cb(id, block_trans_handler);
    return 0;
}
