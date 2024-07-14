#include "config.h"
#include "entry.h"

#define TEST_SCENE 5

#if TEST_SCENE == 0
VM_IMAGE(vm1, "./image/app-helloworld_kvm-arm64");
#elif TEST_SCENE == 1
VM_IMAGE(vm1, "./image/1.hypercall.bin");
#elif TEST_SCENE == 2
VM_IMAGE(vm1, "./image/2.restore-to-latest-ckpt.bin");
#elif TEST_SCENE == 3
VM_IMAGE(vm1, "./image/3.restore-to-specified-ckpt.bin");
#elif TEST_SCENE == 4
VM_IMAGE(vm1, "./image/4.restart.bin");
#elif TEST_SCENE == 5
VM_IMAGE(vm1, "./image/5.1.mem-reader.bin");
VM_IMAGE(vm2, "./image/5.2.mem-writer.bin");
#elif TEST_SCENE == -1
VM_IMAGE(vm1, "./image/output.bin"); // 调试用
#endif

// DTB_IMAGE(dtb1, "./image/virt-gicv3.dtb");
DTB_IMAGE(dtb1, "./image/virt.dtb");

struct config config = {
#if TEST_SCENE != 5
    .hyp = {
        .nr_cpus = 1,
    },
    .nr_vms = 1,
#else
    .hyp = {
        .nr_cpus = 2,
    },
    .nr_vms = 2,
#endif
    .vm = (struct vm_config[]) {
        {
            .base_addr = 0x40100000, 
            .load_addr = VM_IMAGE_OFFSET(vm1),
            .size = VM_IMAGE_SIZE(vm1),
#if TEST_SCENE == 0
            .entry = 0x0000000040101b20,
#else
            .entry = ENTRY_POINT, // 0x0000000040101b20,
#endif
            .dmem_size = 0x8000000, // 128MB
            .nr_cpus = 1,
            .nr_devs = 2,
            .devs = (struct vm_dev_region[]) {
                {
                    .id = 1,
                    .pa = 0x1C0B0000,
                    .va = 0x1C090000,
                    .size = 0x10000,
                    .interrupt_num = 1,
                    .interrupts = (irqid_t[]) {39}
                },
                {
                    .id = 2,
                    .interrupt_num = 1,
                    .interrupts = (irqid_t[]) {27}
                }
            }, 
            .rq_vm = {
                .rq_size = (1024 * 1024),
                .vbase = 0x10000000,
            },
            .arch.gic = {
                .gicd_addr = 0x08000000,
                .gicc_addr = 0x08010000,
                .gicr_addr = 0x080A0000,
            }
        },
#if TEST_SCENE == 5
        {
            .base_addr = 0x40100000, 
            .load_addr = VM_IMAGE_OFFSET(vm2),
            .size = VM_IMAGE_SIZE(vm2),
            .entry = ENTRY_POINT, // 0x0000000040101b20,
            .dmem_size = 0x8000000, // 128MB
            .nr_cpus = 1,
            .nr_devs = 0,
            .rq_vm = {
                .rq_size = (1024 * 1024),
                .vbase = 0x10000000,
            },
            .arch.gic = {
                .gicd_addr = 0x08000000,
                .gicc_addr = 0x08010000,
                .gicr_addr = 0x080A0000,
            }
        }
#endif
    },
    .dtb = {
        .base_addr = 0x40000000,
        .load_addr = DTB_IMAGE_OFFSET(dtb1),
        .size = DTB_IMAGE_SIZE(dtb1),
    }
};