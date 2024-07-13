#include "config.h"

// TODO
// VM_IMAGE(vm1, "./image/app-helloworld_kvm-arm64");
VM_IMAGE(vm1, "./image/hello.bin");

// DTB_IMAGE(dtb1, "./image/virt-gicv3.dtb");
DTB_IMAGE(dtb1, "./image/virt.dtb");

struct config config = {
    .hyp = {
        .nr_cpus = 1,
    },
    .nr_vms = 1,
    .vm = (struct vm_config[]) {
        {
            .base_addr = 0x40100000, 
            .load_addr = VM_IMAGE_OFFSET(vm1),
            .size = VM_IMAGE_SIZE(vm1),
            .entry = 0x0000000040101430, // 0x0000000040101b20,
            .dmem_size = 0x8000000,
            .nr_cpus = 1,
            .nr_devs = 2,
            .devs = (struct vm_dev_region[]) {
                {
                    .id = 1,
                    .pa = 0x1C0B0000,
                    .va = 0x1c090000,
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
        }
    },
    .dtb = {
        .base_addr = 0x40000000,
        .load_addr = DTB_IMAGE_OFFSET(dtb1),
        .size = DTB_IMAGE_SIZE(dtb1),
    }
};