#ifndef UTIL_H
#define UTIL_H

#include "mem_cfg.h"

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "types.h"
#include "printk.h"

#define MAX_VM_NUM      8
#define MAX_VCPU_PER_VM 4
#define MAX_VCPU_NUM    ((MAX_VM_NUM) * (MAX_VCPU_PER_VM))

#define STR(s)  #s
#define XSTR(s)  STR(s)

#define ALIGN(VAL, TO) ((((VAL) + (TO)-1) / (TO)) * TO)
#define IS_ALIGNED(VAL, TO) (!((VAL)%(TO)))
#define NUM_PAGES(SZ) (ALIGN(SZ, PAGE_SIZE)/PAGE_SIZE)

#define ASM __asm__ volatile

#if 1
#define INFO(args, ...) \
    printk("AVISOR INFO: " args "\n" __VA_OPT__(, ) __VA_ARGS__);

#define WARNING(args, ...) \
    printk("AVISOR WARNING: " args "\n" __VA_OPT__(, ) __VA_ARGS__);

#define ERROR(args, ...)                                            \
    {                                                               \
        printk("AVISOR ERROR: " args "\n" __VA_OPT__(, ) __VA_ARGS__); \
        while (1)                                                   \
            ;                                                       \
    }
#else
#define INFO(args, ...) do{}while(0)
#define WARNING(args, ...) do{}while(0)
#define ERROR(args, ...) do{}while(0)
#endif

#if 1
#define ASSERT(expression) \
    do { \
        if (!(expression)) { \
            printk("Assertion failed: %s, file %s, line %d\n", #expression, __FILE__, __LINE__); \
            while(1); \
        } \
    } while (0)
#else
#define ASSERT(expression) do{}while(0)
#endif

#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#define MIN(a, b)		((a) <= (b) ? (a) : (b))

// 优化__sleep函数

// 读取计时器频率（单位：Hz）
static inline uint64_t read_cntfrq_el0(void) {
    uint64_t cntfrq;
    asm volatile ("mrs %0, cntfrq_el0" : "=r" (cntfrq));
    return cntfrq;
}

// 读取当前计时器计数值
static inline uint64_t read_cntpct_el0(void) {
    uint64_t cntpct;
    asm volatile ("mrs %0, cntpct_el0" : "=r" (cntpct));
    return cntpct;
}

// 延迟函数，单位为毫秒
static void inline __sleep(unsigned int milliseconds) {
    uint64_t freq = read_cntfrq_el0();       // 获取计时器频率
    uint64_t start = read_cntpct_el0();      // 获取开始计数值
    uint64_t delay_ticks = (freq / 1000) * milliseconds; // 计算延迟所需的计数值

    while (read_cntpct_el0() - start < delay_ticks);
}

// classic sleep
static void inline __sleep_classic(unsigned int x) {
    uint64_t a = 100000000ul * x;
    uint64_t i = 0;
    
    while(i++ < a);
}


static inline bool range_in_range(unsigned long base1, unsigned long size1,
    unsigned long base2, unsigned long size2) {

    unsigned long limit1 = base1 + size1;
    unsigned long limit2 = base2 + size2;

    /* Saturate possible overflows */
    if (limit1 < base1)  {
        limit1 = ULONG_MAX;
    }
    if (limit2 < base2) {
        limit2= ULONG_MAX;
    }

    return (base1 >= base2) && (limit1 <= limit2);
}

/* WARNING! does not check for overflow! */
#define in_range(_addr, _base, _size) range_in_range(_addr, 0, _base, _size)

#endif

#endif