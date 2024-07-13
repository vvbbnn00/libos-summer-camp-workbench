#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include "util.h"
#include "sysregs.h"

struct cpu_arch {
    SREG64 mpidr;
};

// 获取当前cpu的指针
static inline struct cpu* cpu() {
    size_t base = sysreg_tpidr_el2_read(); // 读取TPIDR_EL2寄存器的值，TPIDR_EL2寄存器存储当前CPU的指针
    return (struct cpu*) base; // 返回当前CPU的指针
}

unsigned long cpu_id_to_mpidr(cpuid_t id);

#endif