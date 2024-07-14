#include "interrupts.h"
#include "vm.h"
#include "config.h"
#include "sysregs.h"
#include "fences.h"
#include "mem.h"
#include "string.h"
#include "cpu.h"
#include "list.h"
#include "sched.h"
// #include "rq.h"

struct vm_list vm_list;

// 初始化虚拟机的地址空间
static void vm_init_mem_regions(struct vm* vm, const struct vm_config* vm_config) { 
    vaddr_t va;
    paddr_t pa;

    // 分配虚拟机的内存空间
    va = mem_alloc_map(&vm->as, NULL, vm_config->base_addr, NUM_PAGES(vm_config->size + vm_config->dmem_size), PTE_VM_FLAGS);
    if (va != vm_config->base_addr) {
        ERROR("va != vm's base_addr");
    }
    mem_translate(&vm->as, va, &pa); // 将虚拟地址转换为物理地址
    memcpy((void*)pa, (void*)vm_config->load_addr, vm_config->size);
    INFO("Copy vm%d to 0x%x, size = 0x%x", vm->id, pa, vm_config->size);
        
    va = mem_alloc_map(&vm->as, NULL,
                (vaddr_t)config.dtb.base_addr, NUM_PAGES(config.dtb.size), PTE_VM_FLAGS);
    if (va != config.dtb.base_addr) {
        ERROR("va != config->vm.base_addr");
    }
    mem_translate(&vm->as, va, &pa);
    memcpy((void*)pa, (void*)config.dtb.load_addr, config.dtb.size);
    INFO("Copy dtb to 0x%x, size = 0x%x", pa, config.dtb.size);
}

// 初始化虚拟机对象
static struct vm* vm_allocation_init(struct vm_allocation* vm_alloc) {
    struct vm *vm = vm_alloc->vm;
    vm->vcpus = vm_alloc->vcpus;
    return vm;
}

// 初始化虚拟机的CPU
void vm_cpu_init(struct vm* vm) {
    spin_lock(&vm->lock);
    vm->cpus |= (1UL << cpu()->id);
    spin_unlock(&vm->lock);
}

static vcpuid_t vm_calc_vcpu_id(struct vm* vm) {
    vcpuid_t vcpu_id = 0;
    for(size_t i = 0; i < cpu()->id; i++) {
        if (!!bit_get(vm->cpus, i)) vcpu_id++;
    }
    return vcpu_id;
}

// 初始化虚拟机的VCPUs（VCPU是虚拟CPU）
void vm_vcpu_init(struct vm* vm, const struct vm_config* vm_config) {
    vcpuid_t vcpu_id = vm_calc_vcpu_id(vm);
    struct vcpu* vcpu = vm_get_vcpu(vm, vcpu_id);

    vcpu->id = vcpu_id;
    vcpu->p_id = cpu()->id;
    vcpu->vm = vm;
    cpu()->vcpu = vcpu;

    vcpu_arch_init(vcpu, vm);
    vcpu_arch_reset(vcpu, vm_config->entry); // 将VCPUs的PC设置为虚拟机的入口地址
}

static void vm_master_init(struct vm* vm, const struct vm_config* vm_config, vmid_t vm_id) {
    vm->master = cpu()->id;
    vm->nr_cpus = vm_config->nr_cpus;
    vm->id = vm_id;
    vm->vm_config = vm_config;

    cpu_sync_init(&vm->sync, vm->nr_cpus);

    as_init(&vm->as, AS_VM, vm_id, NULL);

    INIT_LIST_HEAD(&vm->emul_mem_list);
    INIT_LIST_HEAD(&vm->emul_reg_list);
}

// 初始化虚拟机设备
static void vm_init_dev(struct vm* vm, const struct vm_config* config) {
    for (size_t i = 0; i < config->nr_devs; i++) { // 遍历虚拟机的设备
        struct vm_dev_region* dev = &config->devs[i];

        size_t n = ALIGN(dev->size, PAGE_SIZE) / PAGE_SIZE;

        if (dev->va != INVALID_VA) {
            mem_alloc_map_dev(&vm->as, (vaddr_t)dev->va, dev->pa, n); // 分配设备的内存空间
        }

        for (size_t j = 0; j < dev->interrupt_num; j++) {
            interrupts_vm_assign(vm, dev->interrupts[j]); // 分配中断
        }
    }

    if (io_vm_init(vm, config)) { // 初始化虚拟机的IOMMU，IOMMU是输入输出内存管理单元
        for (size_t i = 0; i < config->nr_devs; i++) {
            struct vm_dev_region* dev = &config->devs[i];
            if (dev->id) {
                if (!io_vm_add_device(vm, dev->id)){ // 添加设备到IOMMU
                    ERROR("Failed to add device to iommu");
                }
            }
        }
    }
      
}

void __print_vm_config(const struct vm_config* config) {
    INFO("VM CONFIG: base_addr=0x%x, size=0x%x, dmem_size=0x%x, entry=0x%x, nr_cpus=%d, nr_devs=%d",
        config->base_addr, config->size, config->dmem_size, config->entry, config->nr_cpus, config->nr_devs);
    for (size_t i = 0; i < config->nr_devs; i++) {
        INFO("DEV[%d]: va=0x%x, pa=0x%x, size=0x%x, interrupt_num=%d, id=%d",
            i, config->devs[i].va, config->devs[i].pa, config->devs[i].size, config->devs[i].interrupt_num, config->devs[i].id);
        for (size_t j = 0; j < config->devs[i].interrupt_num; j++) {
            INFO("INTERRUPT[%d]: %d", j, config->devs[i].interrupts[j]);
        }
    }
}

struct vm* vm_init(struct vm_allocation* vm_alloc, const struct vm_config* vm_config, bool master, vmid_t vm_id) {
    struct vm *vm = vm_allocation_init(vm_alloc);
    
    if (master) {
        vm_master_init(vm, vm_config, vm_id); // 初始化虚拟机的主CPU
    }

    vm_cpu_init(vm);
    cpu_sync_barrier(&vm->sync);
    vm_vcpu_init(vm, vm_config);
    cpu_sync_barrier(&vm->sync);
    vm_arch_init(vm, vm_config);

    if (master) {
        __print_vm_config(vm_config);
        vm_init_mem_regions(vm, vm_config);
        INFO("VM[%d] MEM INIT", vm_id);
        vm_init_dev(vm, vm_config);
        // init address space first
        vm_rq_init(vm, vm_config);
    }

#ifdef SCHEDULE
    task_struct_init(vm);
#endif

    cpu_sync_barrier(&vm->sync);

    INIT_LIST_HEAD(&vm->list);
    spin_lock(&vm_list.lock);
    list_add(&vm->list, &vm_list.list); // 将虚拟机添加到虚拟机列表
    spin_unlock(&vm_list.lock);

    return vm;
}

void vcpu_run(struct vcpu* vcpu) {
    vcpu_arch_run(vcpu);
}

void vm_msg_broadcast(struct vm* vm, struct cpu_msg* msg) {
    for (size_t i = 0, n = 0; n < vm->nr_cpus - 1; i++) {
        if (((1U << i) & vm->cpus) && (i != cpu()->id)) {
            n++;
            cpu_send_msg(i, msg); // 发送消息
        }
    }
}

__attribute__((weak)) cpumap_t vm_translate_to_pcpu_mask(struct vm* vm,
                                                         cpumap_t mask,
                                                         size_t len) {
    cpumap_t pmask = 0;
    cpuid_t shift;
    for (size_t i = 0; i < len; i++) {
        if ((mask & (1ULL << i)) &&
            ((shift = vm_translate_to_pcpuid(vm, i)) != INVALID_CPUID)) {
            pmask |= (1ULL << shift);
        }
    }
    return pmask;
}

__attribute__((weak)) cpumap_t vm_translate_to_vcpu_mask(struct vm* vm,
                                                         cpumap_t mask,
                                                         size_t len) {
    cpumap_t pmask = 0;
    vcpuid_t shift;
    for (size_t i = 0; i < len; i++) {
        if ((mask & (1ULL << i)) &&
            ((shift = vm_translate_to_vcpuid(vm, i)) != INVALID_CPUID)) {
            pmask |= (1ULL << shift);
        }
    }
    return pmask;
}

void vm_emul_add_mem(struct vm* vm, struct emul_mem* emu) {
    list_add_tail(&emu->list, &vm->emul_mem_list);
}

void vm_emul_add_reg(struct vm* vm, struct emul_reg* emu) {
    list_add_tail(&emu->list, &vm->emul_reg_list);
}    

emul_handler_t vm_emul_get_mem(struct vm* vm, vaddr_t addr) {
    struct emul_mem* emu = NULL;
    emul_handler_t handler = NULL;

    list_for_each_entry(emu, &vm->emul_mem_list, list) {
        if (addr >= emu->va_base && (addr < (emu->va_base + emu->size))) {
            handler = emu->handler;
            break;
        }
    }
    return handler;
}

emul_handler_t vm_emul_get_reg(struct vm* vm, vaddr_t addr) {
    struct emul_reg* emu = NULL;
    emul_handler_t handler = NULL;

    // list_foreach(vm->emul_reg_list, struct emul_reg, emu) {
    //     if(emu->addr == addr) {
    //         handler = emu->handler;
    //         break; 
    //     }
    // }
    list_for_each_entry(emu, &vm->emul_reg_list, list) {
        if (emu->addr == addr) {
            handler = emu->handler;
            break;
        }
    }

    return handler;
}

struct vm* get_vm_by_id(vmid_t id) {
    struct vm* res_vm = NULL;

    list_for_each_entry(res_vm, &vm_list.list, list) {
        if (res_vm->id == id) {
            return res_vm;
        }
    }
    return NULL;
}