#ifndef MEM_CFG_H
#define MEM_CFG_H

#define PAGE_SIZE       0x1000          // 4KB
#define STACK_SIZE      PAGE_SIZE       // 栈大小为一页
#define STACK_OFF       PAGE_SIZE       // 栈偏移为一页
#define CPU_SIZE        2 * PAGE_SIZE   // 每个 CPU 占用 2 页

#define MAX_NUM_CPU     8
#define AVISOR_BASE     0x40000000      // AVISOR 的基地址
#define CPU_BASE        0x50000000      // CPU 的基地址，每个 CPU 占用 2 页
#define VM_BASE         (CPU_BASE + (MAX_NUM_CPU * CPU_SIZE)) // VM 的基地址
#define VM_SIZE         0x78000000      // size: 1.5GB, 不能超过2GB，否则会造成地址空间不足
#define RQ_REGION_SIZE  0x10000000      // size: 256MB
#define SHARED_MEM_SIZE 0x1000000       // size: 16MB
#define SHARED_MEM_BASE 0x70000000      // 共享内存的基地址(虚拟地址)

#define CPU_STACK_OFF   PAGE_SIZE
#define CPU_STACK_SIZE  PAGE_SIZE
#define CPU_VCPU_OFF    16
#define VCPU_REGS_OFF   16

#endif
