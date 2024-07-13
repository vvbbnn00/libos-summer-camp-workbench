#include <stdio.h>

/* Import user configuration: */
#ifdef __Unikraft__
#include <uk/config.h>
#endif /* __Unikraft__ */

#define HYPERCALL_ISS_PRINT_MESSAGE "3"

void hypercall_print_message(char *message) {
    register unsigned long x0 __asm__("x0") = (unsigned long)message;
    
    __asm__ __volatile__(
        "hvc #" HYPERCALL_ISS_PRINT_MESSAGE "\n"
        : // No output operand
        : "r"(x0) // Input operand list
        : "memory", "cc"   			// Clobbered registers
    );
}

int main() {
    #define ACTION 1

	printf("Ciallo World, now perform action %d\n", ACTION);

	if (ACTION == 1) {
		hypercall_print_message("Hello, Hypervisor!");
	}
    return 0;
}