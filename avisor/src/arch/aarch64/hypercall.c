#include "hypercall.h"
#include "util.h"
#include "lcm.h"
#include "rq.h"

void print_message_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    INFO("Hypercall message, arg0: %lu, arg1: %lu, arg2: %lu", arg0, arg1, arg2);
    // 打印消息，假设 arg0 是消息的地址
    INFO("Hypercall message: %s", (char *)arg0);
}

hypercall_handler_t hypercall_handlers[8] = {
    [HYPERCALL_ISS_HALT] = guest_halt_handler,
    [HYPERCALL_ISS_CHECKPOINT_SNAPSHOT] = checkpoint_snapshot_handler,  // 创建快照
    [HYPERCALL_ISS_RESTORE_SNAPSHOT] = restore_snapshot_handler,        // 恢复快照
    [HYPERCALL_ISS_PRINT_MESSAGE] = print_message_handler,              // 注册自定义的 Handler
    [HYPERCALL_ISS_RESTART] = restart_vm_handler,                       // 重启虚拟机
};

void hypercall_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    if (iss < 8 && hypercall_handlers[iss]) {
        hypercall_handlers[iss](iss, arg0, arg1, arg2);
    } else {
        INFO("Unknown hypercall iss: %lu", iss);
    }
}