#ifndef HYPERCALL_H
#define HYPERCALL_H

typedef enum {
    // Halt
    HYPERCALL_ISS_HALT = 0,
    // Snapshot
    HYPERCALL_ISS_CHECKPOINT_SNAPSHOT, // 1
    HYPERCALL_ISS_RESTORE_SNAPSHOT, // 2
    HYPERCALL_ISS_PRINT_MESSAGE, // 自定义的Hypercall类型,3
    // Restart
    HYPERCALL_ISS_RESTART, // 自定义的Hypercall类型,4
} HYPERCALL_TYPE;

typedef void (*hypercall_handler_t)(unsigned long, unsigned long, unsigned long, unsigned long);

void hypercall_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);

#endif