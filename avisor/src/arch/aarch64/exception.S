// QUESTION：1、为什么VM_EXIT在保存寄存器的时候，不动态更新sp，但是在SAVE_CPU中却要动态更新sp？
// 

#include "mem_cfg.h"
.section "vectors"

// VM_EXIT： 退出虚拟机时保存 CPU 状态
.macro VM_EXIT
	// sp = &cpu_reg

	// 将x0-x30寄存器的值存储到栈中，一个寄存器的大小是8字节，所以需要存储30个寄存器需要240字节的空间
	stp	x0, x1,   [sp, #0]
	stp	x2, x3,   [sp, #16]
	stp	x4, x5,   [sp, #32]
	stp	x6, x7,   [sp, #48]
	stp	x8, x9,   [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x16, x17, [sp, #128]
	stp	x18, x19, [sp, #144]
	stp	x20, x21, [sp, #160]
	stp	x22, x23, [sp, #176]
	stp	x24, x25, [sp, #192]
	stp	x26, x27, [sp, #208]
	stp	x28, x29, [sp, #224]
	str x30,      [sp, #240]

	// 保存当前异常级别的状态寄存器 (spsr_el2) 
	mrs x0, spsr_el2
	str x0, 	  [sp, #248]
	// 保存异常返回地址寄存器 (elr_el2)
	mrs x0, elr_el2
    stp x0, x1,   [sp, #256]
	
	/* tpidr_el2
	   - 存储当前虚拟机监控器（hypervisor）中当前线程或进程的标识符。
	   - 存储指向当前线程或进程特定数据结构的指针，以便快速访问这些数据。
	
	通过读取 tpidr_el2 寄存器中的基地址，加上堆栈的偏移量和大小，
	计算出当前线程/进程堆栈的顶端地址，并将其存储到堆栈指针 sp 中。这样可
	以确保后续的堆栈操作在正确的位置进行。
	*/

	// 读取线程ID寄存器 (tpidr_el2) 到寄存器 x0 
	mrs x0, tpidr_el2
    // 加载一个常量到寄存器 x1
    ldr x1, =(CPU_STACK_OFF + CPU_STACK_SIZE)
    // 将寄存器 x1 的值加到 x0 上
    add x0, x0, x1
    // 将寄存器 x0 的值存储到堆栈指针 sp 中
    mov sp, x0
	
.endm


// SAVE_CPU： 保存 CPU 状态
.macro SAVE_CPU
	// 存储各个寄存器的状态到栈中，同时更新栈指针 sp
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x8, x9, [sp, #-16]!
	stp	x10, x11, [sp, #-16]!
	stp	x12, x13, [sp, #-16]!
	stp	x14, x15, [sp, #-16]!
	stp	x16, x16, [sp, #-16]!
	stp	x18, x19, [sp, #-16]!
	stp	x20, x21, [sp, #-16]!
	stp	x22, x23, [sp, #-16]!
	stp	x24, x25, [sp, #-16]!
	stp	x26, x27, [sp, #-16]!
	stp	x28, x29, [sp, #-16]!
	str x30, [sp, #-8]!

	// 读取当前异常级别的状态寄存器 (spsr_el2) 到寄存器 x0
	mrs x0, spsr_el2
	str x0, [sp, #-8]!

	// 读取异常返回地址寄存器 (elr_el2) 到寄存器 x0
	mrs x0, elr_el2
	str x0, [sp, #-8]!
.endm

// 全局符号 vector
.global vector
vector:

// --------- EL0 异常处理程序 ------------
// 异常向量表
sp_el0_sync:        b       .   // EL0 同步异常
.align 7 , 0xff
sp_el0_irq:         b       .   // EL0 中断
.align 7 , 0xff
sp_el0_fiq:         b       .   // EL0 快速中断
.align 7 , 0xff
sp_el0_serror:      b       .   // EL0 同步错误

// --------- ELx 异常处理程序 ------------
.align 7 , 0xff
sp_elx_sync:        b       .   // 高级异常级别的同步异常
.align 7 , 0xff
sp_elx_irq:                    // 高级异常级别的中断
    SAVE_CPU                // 保存 CPU 状态
    bl gic_handle           // 跳转到 GIC 处理函数
    b restore_cpu           // 跳转到恢复 CPU 状态函数
.align 7 , 0xff
sp_elx_fiq:         b       .   // 高级异常级别的快速中断
.align 7 , 0xff
sp_elx_serror:      b       .   // 高级异常级别的同步错误

// --------- EL1 异常处理程序 (64 位) ------------
.align 7 , 0xff
lower_64_sync:                // 64 位下的同步异常
    VM_EXIT                  // 执行 VM_EXIT 宏
    bl aborts_sync_handler   // 跳转到同步异常处理函数
    b vm_entry               // 跳转到虚拟机入口
.align 7 , 0xff
lower_64_irq:                 // 64 位下的中断
    VM_EXIT                  // 执行 VM_EXIT 宏
    bl gic_handle            // 跳转到 GIC 处理函数
    // TODO: 放置 task_struct 到栈顶后修改此处
    bl try_reschedule        // 跳转到重新调度函数
    b vm_entry               // 跳转到虚拟机入口
.align 7 , 0xff
lower_64_fiq:       b       .   // 64 位下的快速中断
.align 7 , 0xff
lower_64_serror:    b       .   // 64 位下的同步错误

// --------- EL1 异常处理程序 (32 位) ------------
.align 7 , 0xff
lower_32_sync:      b       .   // 32 位下的同步异常
.align 7 , 0xff
lower_32_irq:       b       .   // 32 位下的中断
.align 7 , 0xff
lower_32_fiq:       b       .   // 32 位下的快速中断
.align 7 , 0xff
lower_32_serror:    b       .   // 32 位下的同步错误
// ----------------------------------------

// vm_entry：虚拟机入口，参数是 VCPU 的寄存器偏移量
.global vm_entry
vm_entry:
	// 把当前的 EL 级别设置为 EL2
	mrs x0, tpidr_el2
	// 从 x0 中读取虚拟 CPU 的偏移量，然后加上 CPU_VCPU_OFF，得到虚拟 CPU 的地址
	ldr x0, [x0, #CPU_VCPU_OFF]
	// 将寄存器偏移量加到 x0 上，得到 VCPU 的地址
	add x0, x0, #VCPU_REGS_OFF
	// 将寄存器 x0 的值存储到堆栈指针 sp 中，这样可以确保后续的堆栈操作在正确的位置进行
	mov sp, x0

	// 加载 spsr_el2 寄存器的值
	ldr	x0, [sp, #248]
	msr	spsr_el2, x0

	// 加载 elr_el2 寄存器的值
	ldr	x0, [sp , #256]
    msr	elr_el2, x0		

	// 从栈中依次加载 x0-x30 寄存器的值
	ldp	x0, x1,   [sp, #0]
	ldp	x2, x3,   [sp, #16]
	ldp	x4, x5,   [sp, #32]
	ldp	x6, x7,   [sp, #48]
	ldp	x8, x9,  [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x16, x17, [sp, #128]
	ldp	x18, x19, [sp, #144]
	ldp	x20, x21, [sp, #160]
	ldp	x22, x23, [sp, #176]
	ldp	x24, x25, [sp, #192]
	ldp	x26, x27, [sp, #208]
	ldp	x28, x29, [sp, #224]
	ldr	x30, [sp, #240]

    // 异常返回到 EL2
    eret

// restore_cpu：恢复 CPU 状态
.global restore_cpu
restore_cpu:        
    // 加载 elr_el2 寄存器的值
    ldr	x0, [sp], #8
    msr	elr_el2, x0 

    // 加载 spsr_el2 寄存器的值
    ldr	x0, [sp], #8
    msr	spsr_el2, x0

    // 依次加载 x0-x30 寄存器的值
    ldr	x30, 	  [sp], #8 
	ldp	x29, x28, [sp], #16 
	ldp	x27, x26, [sp], #16 
	ldp	x25, x24, [sp], #16 
	ldp	x23, x22, [sp], #16 
	ldp	x21, x20, [sp], #16
	ldp	x19, x18, [sp], #16
	ldp	x17, x16, [sp], #16
	ldp	x15, x14, [sp], #16
	ldp	x13, x12, [sp], #16
	ldp	x11, x10, [sp], #16
	ldp	x9, x8, [sp], #16
	ldp	x7, x6, [sp], #16
	ldp	x5, x4, [sp], #16
	ldp	x3, x2, [sp], #16
	ldp	x1, x0, [sp], #16

    // 异常返回到 EL2
    eret