#ifndef LCM_H
#define LCM_H

#include "types.h"
#include "list.h"
#include "vm.h"

#define NUM_MAX_SNAPSHOT_RESOTRE        1
#define LATEST_SSID                    -1
#define NUM_MAX_SNAPSHOT_PER_POOL       3

//TODO: 移动到psci.h
#define PSCI_FNID_SYSTEM_OFF            0x84000008
#define PSCI_FNID_SYSTEM_RESET          0x84000009


struct snapshot_pool {
    paddr_t base;
    size_t size;
    paddr_t last;
    struct list_head list;
    char pool[0];
};

struct snapshot {
    ssid_t ss_id;
    size_t size;
    uint32_t vm_id;
    struct vcpu vcpu;
    char mem[0];
};

void checkpoint_snapshot_handler(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec);
void restore_snapshot_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);
void guest_halt_handler(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec);
void restart_vm_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);

extern struct list_head ss_pool_list;

static struct snapshot_pool* alloc_ss_pool();
void restore_snapshot_handler_by_ss(struct snapshot* ss);

#endif