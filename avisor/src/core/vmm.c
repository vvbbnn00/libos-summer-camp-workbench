#include "util.h"
#include "vmm.h"
#include "vm.h"
#include "string.h"
#include "config.h"
#include "sysregs.h"
#include "vmm.h"
#include "cpu.h"
#include "io.h"
#include "interrupts.h"
// #include "rq.h"

static struct vm_assignment {
    spinlock_t lock;
    bool master;
    size_t ncpus;
    cpumap_t cpus;
    struct vm_allocation vm_alloc;
} vm_assign[MAX_VM_NUM];

static bool vmm_assign_vcpu(bool *master, vmid_t *vm_id) {
    bool assigned = false;
    *master = false;

    for (size_t i = 0; i < config.nr_vms && !assigned; i++) {
        spin_lock(&vm_assign[i].lock);
        if (vm_assign[i].ncpus < config.vm[i].nr_cpus) {
            if (!vm_assign[i].master) {
                vm_assign[i].master = true;
                vm_assign[i].ncpus++;
                *master = true;
                assigned = true;
                vm_assign[i].cpus |= (1UL << cpu()->id);
                *vm_id = i;
            } else {
                assigned = true;
                vm_assign[i].ncpus++;
                vm_assign[i].cpus |= (1UL << cpu()->id);
                *vm_id = i;
            }
        }
        spin_unlock(&vm_assign[i].lock);
    }

    return assigned;
}

static bool vmm_alloc_vm(struct vm_allocation* vm_alloc, struct vm_config *config) {

    /**
     * We know that we will allocate a block aligned to the PAGE_SIZE, which
     * is guaranteed to fulfill the alignment of all types.
     * However, to guarantee the alignment of all fields, when we calculate 
     * the size of a field in the vm_allocation struct, we must align the
     * previous total size calculated until that point, to the alignment of 
     * the type of the next field.
     * 
     * 我们知道我们将分配一个按PAGE_SIZE对齐的块，这保证了所有类型的对齐。
     * 但是，为了保证所有字段的对齐，当我们计算vm_allocation结构中字段的大小时，
     * 我们必须将到目前为止计算的总大小对齐到下一个字段的类型的对齐。
     */

    size_t total_size = sizeof(struct vm);
    size_t vcpus_offset = ALIGN(total_size, _Alignof(struct vcpu));
    total_size = vcpus_offset + (config->nr_cpus * sizeof(struct vcpu));
    total_size = ALIGN(total_size, PAGE_SIZE);

    void* allocation = mem_alloc_page(NUM_PAGES(total_size), false);
    if (allocation == NULL) {
        ERROR("Failed to allocate memory for VM.");
        return false;
    }
    memset((void*)allocation, 0, total_size);

    vm_alloc->base = (vaddr_t) allocation;
    vm_alloc->size = total_size;
    vm_alloc->vm = (struct vm*) vm_alloc->base;
    vm_alloc->vcpus = (struct vcpu*) (vm_alloc->base + vcpus_offset);

    INFO("VM memory allocated at %x with size 0x%x", allocation, total_size);
    return true;
}

// 分配一个虚拟机，初始化虚拟机的内部结构
static struct vm_allocation* vmm_alloc_install_vm(vmid_t vm_id, bool master) {
    struct vm_allocation *vm_alloc = &vm_assign[vm_id].vm_alloc;
    struct vm_config *vm_config = &config.vm[vm_id];

    if (master) {
        if (!vmm_alloc_vm(vm_alloc, vm_config)) {
            ERROR("Failed to allocate vm internal structures");
        }
        fence_ord_write();
    }

    return vm_alloc;
}

void vmm_io_init() {
    io_init();
}

void vm_list_init() {
    INIT_LIST_HEAD(&vm_list.list);
}

void vmm_init() { 
    vmm_arch_init(); // 初始化虚拟机管理器的体系结构相关的部分    
    vmm_io_init(); // 初始化虚拟机管理器的IO部分    
    // ipc_init() // 如果有IPC初始化，需要确保它正确完成
    cpu_sync_barrier(&cpu_glb_sync); // 等待所有CPU都到达这里    
    vm_list_init(); // 初始化虚拟机列表
    bool master = false; // 是否是主CPU
    vmid_t vm_id = -1;
    if (vmm_assign_vcpu(&master, &vm_id)) { // 分配一个虚拟机，第一次分配的CPU是主CPU
        INFO("VMID:%d assigned. Load addr: 0x%x", vm_id, config.vm[vm_id].load_addr);
        
        struct vm_allocation *vm_alloc = vmm_alloc_install_vm(vm_id, master);
        INFO("vmm_alloc_install_vm completed.");
        
        struct vm_config *vm_config = &config.vm[vm_id];
        struct vm *vm = vm_init(vm_alloc, vm_config, master, vm_id); // 初始化虚拟机（这个虚拟机是被Avisor管理的）
        INFO("vm_init completed for VMID:%d", vm_id);
        
        cpu_sync_barrier(&vm->sync);
        INFO("VM sync barrier passed for VMID:%d", vm_id);

        vcpu_run(cpu()->vcpu); // 运行虚拟机
        INFO("vcpu_run started for VMID:%d", vm_id);
    } else {
        INFO("No VM assigned to this CPU, entering idle state.");
        // 如果这个CPU没有分配到虚拟机，那么就让这个CPU空闲
        cpu_idle();
    }
    INFO("VMM initialization completed.");
}
