#include "lcm.h"
#include "string.h"

#define NUM_MAX_SNAPSHOT_RESOTRE 1

struct snapshot* latest_ss;
ssid_t latest_ss_id = 0;
struct list_head ss_pool_list;
size_t current_restore_cnt = 0;


// 获取一个新的快照结构体指针
static inline struct snapshot* get_new_ss() {
    struct snapshot_pool* ss_pool = list_last_entry(&ss_pool_list, struct snapshot_pool, list);
    // 快照池已满, 分配一个新的快照池
    if (ss_pool->last + sizeof(struct snapshot) > ss_pool->base + ss_pool->size) { 
        struct snapshot_pool* new_ss_pool = alloc_ss_pool();
        if (!new_ss_pool) {
            ERROR("Failed to allocate memory for new snapshot pool.");
            return NULL;
        }
        list_add_tail(&new_ss_pool->list, &ss_pool_list);
        return (struct snapshot*) new_ss_pool->base;
    }
    return (struct snapshot*) ss_pool->last;
}

// 更新快照池的最后指针位置
static inline void update_ss_pool_last(size_t size) {
    struct snapshot_pool* ss_pool = list_last_entry(&ss_pool_list, struct snapshot_pool, list);
    latest_ss = (struct snapshot*) ss_pool->last;
    ss_pool->last += size;
}

// 分配一个新的快照池
static struct snapshot_pool* alloc_ss_pool() {
    struct snapshot_pool* ss_pool;
    size_t pool_size = (config.vm->dmem_size + sizeof(struct snapshot)) * NUM_MAX_SNAPSHOT_RESOTRE;

    INFO("new snapshot pool size: %dMB", pool_size / 1024 / 1024);
    ss_pool = (struct snapshot_pool*) mem_alloc_page(NUM_PAGES(pool_size), false);
    if (!ss_pool) {
        ERROR("Failed to allocate memory for snapshot pool.");
        return NULL;
    }
    
    ss_pool->base = (paddr_t) ss_pool + sizeof(struct snapshot_pool);
    ss_pool->size = pool_size;
    ss_pool->last = ss_pool->base;
    INIT_LIST_HEAD(&ss_pool->list);

    return ss_pool;
}

// 初始化快照池
void ss_pool_init() {
    INIT_LIST_HEAD(&ss_pool_list);
    struct snapshot_pool* ss_pool = alloc_ss_pool();
    list_add_tail(&ss_pool->list, &ss_pool_list);
}

// 获取新的快照ID
static inline ssid_t get_new_ss_id() {
    return latest_ss_id++;
}

// 获取最新的快照
static inline struct snapshot* get_latest_ss() {
    return latest_ss;
}

// 根据快照ID获取快照
static inline struct snapshot* get_ss_by_id(ssid_t id) {
    paddr_t ss;
    struct snapshot_pool* ss_pool;
    int i = 0;

    list_for_each_entry(ss_pool, &ss_pool_list, list) {
        ss = ss_pool->base;
        // feat: 优化快照查找
        while (ss + ((struct snapshot*)ss)->size <= ss_pool->base + ss_pool->size && ((struct snapshot*)ss)->size > 0) {
            if (((struct snapshot*)ss)->ss_id == id) {
                return (struct snapshot*) ss;
            }
            ss += ((struct snapshot*)ss)->size;
        }
    }

    ERROR("invalid snapshot id");
}

// 处理guest的halt hypercall
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
                restore_snapshot_handler(iss, arg0, arg1, arg2);
            } else {
                ERROR("Reach maximum number of restores.");
            }
            break;
        }
        default:
            ERROR("Unknown halt reason: %lu", reason);
            break;
    }
}

// 重启虚拟机
void restart_vm() {
    const struct vm_config* config = CURRENT_VM->vm_config;
    vaddr_t va = config->base_addr;
    paddr_t pa = 0;
    if (!mem_translate(&CURRENT_VM->as, va, &pa)) {
        ERROR("Memory translation failed.");
        return;
    }

    // 恢复内存状态
    memcpy((void*)pa, (void*)config->load_addr, config->dmem_size);
    // 重置vCPU
    vcpu_arch_reset(CURRENT_VM->vcpus, config->entry);

    INFO("Restart vm%d", CURRENT_VM->id);
}


// 重启虚拟机的hypercall
void restart_vm_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    restart_vm();
}


// 恢复快照的hypercall
void restore_snapshot_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) { 
    // arg0: ssid, if arg0 == LATEST_SSID then restore the latest snapshot
    ssid_t ssid = arg0;
    struct snapshot* ss;

    INFO("Asked snapshot: ID=%d", ssid);

    if (ssid == LATEST_SSID) {
        ss = get_latest_ss();
    } else {
        ss = get_ss_by_id(ssid);
    }

    if (ss == NULL) {
        ERROR("No such snapshot. (ssid=%lu)", ssid);
        return;
    }

    restore_snapshot_handler_by_ss(ss);
    INFO("Restore snapshot: ID=%lu, size=%lu", ss->ss_id, ss->size);
}

unsigned int __hash_object(const void* obj, size_t size) {
    unsigned int hash = 0;
    const unsigned char* p = (const unsigned char*) obj;
    for (size_t i = 0; i < size; i++) {
        hash = 31 * hash + p[i];
    }
    return hash;
}

void __print_regs(struct vcpu vcpu ) {
    for (int i = 0; i < 32; i++) {
        INFO("x%d: 0x%lx", i, vcpu_readreg(&vcpu, i));
    }
}

// 创建快照的hypercall
void checkpoint_snapshot_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    static bool init = false;
    struct snapshot* ss;
    paddr_t pa;
    const struct vm_config* config = CURRENT_VM->vm_config;

    if (!init) {
        ss_pool_init();
        init = true;
        INFO("Snapshot pool initialized.");
    }

    // 要实现创建快照的功能，需要完成以下几个步骤：
    // 1. 为快照分配内存空间
    ss = get_new_ss();
    if (ss == NULL) {
        ERROR("Failed to allocate memory for snapshot.");
        return;
    }
    
    ss->ss_id = get_new_ss_id();
    ss->size = sizeof(struct snapshot) + config->dmem_size;
    ss->vm_id = CURRENT_VM->id;
    INFO("Create snapshot: ID=%lu", ss->ss_id);

    // 2. 保存vcpu的状态
    memcpy(&ss->vcpu, cpu()->vcpu, sizeof(struct vcpu));
    INFO("Save vcpu state, pc=0x%lx", vcpu_readpc(cpu()->vcpu));
    // __print_regs(ss->vcpu);

    // 3. 保存内存状态
    vaddr_t va = config->base_addr;
    pa = 0;
    mem_translate(&CURRENT_VM->as, va, &pa);
    if (pa == 0) {
        ERROR("Memory translation failed.");
        return;
    }

    memcpy(ss->mem, (void*)pa, config->dmem_size);
    
    INFO("[checkpoint] Ckpt hash: %x", __hash_object(ss, ss->size));
    // 4. 更新快照池的最后指针位置
    update_ss_pool_last(ss->size);

    INFO("Checkpoint snapshot created: ID=%lu, size=%lu", ss->ss_id, ss->size);
}

/**
 * 恢复为给定快照
 * 
 * @param ss 要恢复的快照
 */
void restore_snapshot_handler_by_ss(struct snapshot* ss) {
    INFO("[restore] Ckpt hash: %x", __hash_object(ss, ss->size));

    // 理论上来说，恢复快照时，cpu的pc应当是快照创建时的pc，
    // 但是为了能够在恢复快照后继续运行，我们将pc设置为发起恢复时的pc
    // uint64_t pc = vcpu_readpc(cpu()->vcpu);


    // 恢复内存状态
    const struct vm_config* config = CURRENT_VM->vm_config;
    vaddr_t va = config->base_addr;
    paddr_t pa = 0;
    if (!mem_translate(&CURRENT_VM->as, va, &pa)) {
        ERROR("Memory translation failed.");
        return;
    }
    memcpy((void*)pa, ss->mem, config->dmem_size);

    // 恢复vcpu的状态
    memcpy(cpu()->vcpu, &ss->vcpu, sizeof(struct vcpu));
    // vcpu_writepc(cpu()->vcpu, pc); // 恢复pc,实际不太合理
    // __print_regs(*(cpu()->vcpu));

    INFO("Restore vcpu state, pc=0x%lx", vcpu_readpc(cpu()->vcpu));
}
