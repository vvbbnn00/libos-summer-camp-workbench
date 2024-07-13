#include <stdio.h>

#define HYPERCALL_ISS_CHECKPOINT_SNAPSHOT "1"
#define HYPERCALL_ISS_RESTORE_SNAPSHOT "2"
#define HYPERCALL_ISS_PRINT_MESSAGE "3"
#define HYPERCALL_ISS_RESTART "4"

#define LATEST_SNAPSHOT -1

void hypercall_print_message(char *message) {
    register unsigned long x0 __asm__("x0") = (unsigned long)message;
    
    __asm__ __volatile__(
        "hvc #" HYPERCALL_ISS_PRINT_MESSAGE "\n"
        : // No output operand
        : "r"(x0) // Input operand list
        : "memory", "cc"   			// Clobbered registers
    );
}

void hypercall_checkpoint_snapshot() {
    __asm__ __volatile__(
        "hvc #" HYPERCALL_ISS_CHECKPOINT_SNAPSHOT "\n"
        : // No output operand
        : // No input operand
        : "memory", "cc"   			// Clobbered registers
    );
}

// x0: snapshot_id
void hypercall_restore_snapshot(unsigned long snapshot_id) {
    __asm__ __volatile__(
        "hvc #" HYPERCALL_ISS_RESTORE_SNAPSHOT "\n"
        : // No output operand
        : "r"(snapshot_id) // Input operand list
        : "memory", "cc"   			// Clobbered registers
    );
}

void hypercall_restart() {
    __asm__ __volatile__(
        "hvc #" HYPERCALL_ISS_RESTART "\n"
        : // No output operand
        : // No input operand
        : "memory", "cc"   			// Clobbered registers
    );
}

int main() {
    #define ACTION 3

	printf("Ciallo World, now perform action %d\n", ACTION);
    // 由于在restore时会回到checkpoint时的状态，所以在restore之后的代码不会被执行，会循环checkpoint之后的代码

	if (ACTION == 1) {
		hypercall_print_message("Hello, Hypervisor!");
	}
    if (ACTION == 2) {
        static int i = 0;
        register int j = 0x114514;
        printf("\n--------- Initialize ---------\n");
        printf("Initial state: i(static)=%d, j(register)=%d\n", i, j);
        hypercall_checkpoint_snapshot();
        printf("After checkpoint, i(static)=%d, j(register)=%d\n", i, j);

        i = 10;
        j = 20;

        printf("After modification, i(static)=%d, j(register)=%d\n\n", i, j);
        printf("--------- Restore snapshot ---------\n");
        hypercall_restore_snapshot(LATEST_SNAPSHOT);
    }
    if (ACTION == 3) {
        static int current = 10;
        
        // --- ckpt #0 ---
        printf("ckpt #0, i=%d\n", current);
        hypercall_checkpoint_snapshot();
        // --- ckpt #1 ---
        current = 20;
        printf("ckpt #1, i=%d\n", current);

        hypercall_checkpoint_snapshot();
        // --- ckpt #2 ---
        current = 30;
        printf("ckpt #2, i=%d\n", current);
        hypercall_checkpoint_snapshot();
        // --- ckpt #3 ---
        current = 40;
        printf("ckpt #3, i=%d\n", current);
        hypercall_checkpoint_snapshot();
        // --- ckpt #4 ---
        current = 50;
        printf("ckpt #4, i=%d\n", current);
        hypercall_checkpoint_snapshot();

        // --- restore #1 ---
        hypercall_restore_snapshot(1);
    }
    if (ACTION == 4) {
        printf("--- Restart ---\n");
        hypercall_restart();
    }
    return 0;
}