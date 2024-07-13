#include "lcm.h"
#include "string.h"

struct snapshot* latest_ss;
ssid_t latest_ss_id = 0;
struct list_head ss_pool_list;
size_t current_restore_cnt = 0;

static inline struct snapshot* get_new_ss() {
    struct snapshot_pool* ss_pool = list_last_entry(&ss_pool_list, struct snapshot_pool, list);
    //TODO: check if enough
    return (struct snapshot*) ss_pool->last;
}

static inline void update_ss_pool_last(size_t size) {
    struct snapshot_pool* ss_pool = list_last_entry(&ss_pool_list, struct snapshot_pool, list);
    latest_ss = (struct snapshot*) ss_pool->last;
    ss_pool->last += size;
}

static struct snapshot_pool* alloc_ss_pool() {
    struct snapshot_pool* ss_pool;
    size_t pool_size = (config.vm->dmem_size + sizeof(struct snapshot)) * 5;

    INFO("new snapshot pool size: %dMB", pool_size / 1024 / 1024);
    
    ss_pool = (struct snapshot_pool*) mem_alloc_page(NUM_PAGES(pool_size), false);
    ss_pool->base = (paddr_t) ss_pool + sizeof(struct snapshot_pool);
    ss_pool->size = pool_size;
    ss_pool->last = ss_pool->base;
    INIT_LIST_HEAD(&ss_pool->list);

    return ss_pool;
}

void ss_pool_init() {
    INIT_LIST_HEAD(&ss_pool_list);
    struct snapshot_pool* ss_pool = alloc_ss_pool();
    list_add_tail(&ss_pool->list, &ss_pool_list);
}

static inline ssid_t get_new_ss_id() {
    return latest_ss_id++;
}

static inline struct snapshot* get_latest_ss() {
    return latest_ss;
}

static inline struct snapshot* get_ss_by_id(ssid_t id) {
    paddr_t ss;
    struct snapshot_pool* ss_pool;
    int i = 0;

    list_for_each_entry(ss_pool, &ss_pool_list, list) {
        ss = ss_pool->base;
        while (i < id && ss + ((struct snapshot*)ss)->size <= ss_pool->base + ss_pool->size) {
            ss += ((struct snapshot*)ss)->size;
            i++;
        }
        if (id == i) {
            return (struct snapshot*) ss;
        }
    }

    ERROR("invalid snapshot id");
}

void guest_halt_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    unsigned long reason = arg0;

    switch (reason) {
        case PSCI_FNID_SYSTEM_OFF:
            INFO("Guest System off %d", cpu()->id);
            break;
        case PSCI_FNID_SYSTEM_RESET: {
            if (current_restore_cnt < NUM_MAX_SNAPSHOT_RESOTRE) {
                INFO("Try to restore latest snapshot.");
                current_restore_cnt++;
                restore_snapshot_hanlder(iss, arg0, arg1, arg2);
            } else {
                ERROR("Reach maximum number of restores.")
            }
            break;
        }
    }
}

void restart_vm() {
    /*
        可以尝试实现
    */
}

void restore_snapshot_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) { 
    // Implement me: 恢复快照的handler
}

void checkpoint_snapshot_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    static bool init = false;
    struct snapshot* ss;
    paddr_t pa;
    const struct vm_config* config = CURRENT_VM->vm_config;

    if (!init) {
        ss_pool_init();
        init = true;
    }

    // Implement me：根据框架中目前提供的快照池等函数实现checkpoint

    
}

void restore_snapshot_hanlder_by_ss(struct snapshot* ss) {
    // 描述: 恢复为给定ss的快照
}