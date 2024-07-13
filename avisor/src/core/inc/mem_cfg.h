#ifndef MEM_CFG_H
#define MEM_CFG_H

#define PAGE_SIZE       0x1000
#define STACK_SIZE      PAGE_SIZE
#define STACK_OFF       PAGE_SIZE
#define CPU_SIZE        2 * (PAGE_SIZE)

#define MAX_NUM_CPU     8
#define AVISOR_BASE     0x40000000  // AVISOR 的基地址
#define CPU_BASE        0x50000000  // CPU 的基地址，每个 CPU 占用 2 页
#define VM_BASE         (CPU_BASE + (MAX_NUM_CPU) * (CPU_SIZE)) // VM 的基地址
#define VM_SIZE         0x20000000     // size: 512M
#define RQ_REGION_SIZE  0x10000000     // size: 256M

#define CPU_STACK_OFF   4096
#define CPU_STACK_SIZE  4096
#define CPU_VCPU_OFF    16
#define VCPU_REGS_OFF   16

#endif