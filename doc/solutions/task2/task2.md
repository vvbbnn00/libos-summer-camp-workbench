## Task2

### 1. Hypercall 是怎么发起和被处理的?

#### Hypercall 的发起
当虚拟机需要与 Hypervisor 通信时，会发起一个 Hypercall。在 AArch64 架构中，Hypercall 是通过执行 `hvc` 指令发起的（一个最简单的例子是在任务文档中给出的 `__asm__ __volatile__("hvc #0x00");`）

`hvc` 指令会触发一个同步异常，这个异常会被 Hypervisor 捕获和处理。

#### Hypercall 的处理
执行 `hvc` 指令后，虚拟机会触发一个同步异常，处理器会切换到异常级别 EL2（通常 Hypervisor 运行在 EL2）并跳转到 EL2 的同步异常向量表。通过阅读 `exception.S` 的源代码，我们不难发现，64 位模式下，处理来自 EL1 的同步异常会跳转到 `lower_64_sync` 函数：
```assembly
.align 7 , 0xff
lower_64_sync:                // 64 位下的同步异常
    VM_EXIT                  // 执行 VM_EXIT 宏
    bl aborts_sync_handler   // 跳转到同步异常处理函数
    b vm_entry               // 跳转到虚拟机入口
```
其中，`VM_EXIT` 宏会保存虚拟机的状态，`aborts_sync_handler` 函数会处理异常（具体的异常处理函数则是在 `aborts.c` 中实现），`vm_entry` 函数会恢复虚拟机的状态。

通过阅读 `VM_EXIT` 宏，我们可以发现，这个宏负责保存当前虚拟机的状态（如寄存器、程序计数器等）到堆栈中。

在 `aborts.c` 中，我们可以找到异常处理函数 `aborts_sync_handler` 的实现：
```c
void aborts_sync_handler() {
    uint64_t hsr = sysreg_esr_el2_read();
    uint64_t ec = bit64_extract(hsr, ESR_EC_OFF, ESR_EC_LEN);
    uint64_t iss = bit64_extract(hsr, ESR_ISS_OFF, ESR_ISS_LEN);
    uint64_t il = bit64_extract(hsr, ESR_IL_OFF, ESR_IL_LEN); // instruction length
    unsigned long far = sysreg_far_el2_read();
    uint64_t arg0, arg1, arg2;

    if (ec == ESR_EC_DALEL) {
        aborts_data_lower(iss, far, il, ec);
    } else if (ec == ESR_EC_HVC64) {
        arg0 = vcpu_readreg(cpu()->vcpu, 0);
        arg1 = vcpu_readreg(cpu()->vcpu, 1);
        arg2 = vcpu_readreg(cpu()->vcpu, 2);
        
        hypercall_handler(iss, arg0, arg1, arg2);
    }
}
```

在这个函数中，我们可以看到，异常处理函数会根据异常码 `ec` 的不同，调用不同的处理函数。当异常码为 `ESR_EC_HVC64` 时，会调用 `hypercall_handler` 函数处理 Hypercall 请求。

`hypercall_handler` 函数在 `hypercall.c` 中实现:
```c

void print_message_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    INFO("Hypercall message, arg0: %lu, arg1: %lu, arg2: %lu", arg0, arg1, arg2);
    // 打印消息，arg0 是消息的地址
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
```
在这个函数中，根据 Hypercall 的 `iss`（Hypercall 指令码）调用对应的 Hypercall 处理函数。这样，虚拟机就能够发起 Hypercall 并由 Hypervisor 处理。

Hypercall 处理完成后，控制权返回到异常处理程序。异常处理程序会执行 `vm_entry`，这个步骤会从堆栈中恢复虚拟机的状态，包括寄存器、程序计数器等。恢复完成后，虚拟机会继续执行被中断的代码。

#### Hypercall 的实验

在实验中，我们实现了一个自定义的 Hypercall 处理函数 `print_message_handler`，用于打印消息。在虚拟机中，我们通过 Hypercall 发起一个打印消息的请求，Hypervisor 会捕获这个请求并打印消息。

![实验结果](image.png)

