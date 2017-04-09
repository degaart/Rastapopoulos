#include "kernel.h"
#include "io.h"
#include "string.h"
#include "debug.h"
#include "registers.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "pmm.h"
#include "util.h"
#include "kmalloc.h"
#include "vmm.h"
#include "syscall.h"
#include "ipc.h"
#include "list.h"
#include "heap.h"
#include "elf.h"
#include "debug.h"

uint32_t KERNEL_START = (uint32_t) &_KERNEL_START_;
uint32_t KERNEL_END = (uint32_t) &_KERNEL_END_;

const char* current_task_name();
int current_task_pid();

static void pf_handler(struct isr_regs* regs)
{
    bool user_mode = regs->err_code & (1 << 2); /* user or supervisor mode */
    bool write = regs->err_code & (1 << 1);     /* was a read or a write */
    bool prot_violation = regs->err_code & 1;   /* not-present page or page protection violation */
    void* address = (void*)read_cr2();
    const char* function = lookup_function(regs->eip);

    trace(
        "Page fault at address %p: %s %s %s\n"
        "\tds:  0x%X\n"
        "\teax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X\n"
        "\tesi: 0x%X edi: 0x%X\n"
        "\terr: 0x%X\n"
        "\tcs:  0x%X eip: 0x%X (%s) eflags: 0x%X\n"
        "\tss:  0x%X esp: 0x%X\n"
        "\tcr3: %p\n"
        "\tcurrent task: %d %s\n",
        address,
        user_mode ? "ring3" : "ring0",
        write ? "write" : "read",
        prot_violation ? "access-violation" : "page-not-present",
        regs->ds,
        regs->eax, regs->ebx, regs->ecx, regs->edx,
        regs->esi, regs->edi,
        regs->err_code,
        regs->cs, regs->eip, function ? function : "??", regs->eflags, 
        regs->ss, regs->esp,
        read_cr3(),
        current_task_pid(), current_task_name() ? current_task_name() : ""
    );
    abort();
}

static void gpf_handler(struct isr_regs* regs)
{
    const char* function = lookup_function(regs->eip);

    trace(
        "General protection fault:\n"
        "\tdescriptor: %p\n"
        "\tds:  0x%X\n"
        "\teax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X\n"
        "\tesi: 0x%X edi: 0x%X\n"
        "\tcs:  0x%X eip: 0x%X (%s) eflags: 0x%X\n"
        "\tss:  0x%X esp: 0x%X\n",
        regs->err_code,
        regs->ds,
        regs->eax, regs->ebx, regs->ecx, regs->edx,
        regs->esi, regs->edi,
        regs->cs, regs->eip, function ? function : "??", regs->eflags, 
        regs->ss, regs->esp
    );
    abort();
}

void reboot()
{
    trace("*** Rebooted ***\n\n\n");

    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    halt();
}

void abort()
{
    backtrace();
    reboot();
}

void halt()
{
    cli();
    hlt();
}

static void reboot_timer(void* data, const struct isr_regs* regs)
{
    trace("*** Rebooting ***\n\n\n");
    reboot();
}


struct initrd_file {
    char name[16];
    void* data;
    unsigned size;
    list_declare_node(initrd_file) node;
};
list_declare(initrd, initrd_file);

static void load_initrd(struct initrd* initrd, const struct multiboot_info* mi)
{
    if(mi->flags & MULTIBOOT_FLAG_MODINFO) {
        trace("Loading initrd");
        const struct multiboot_mod_entry* modules =
            (struct multiboot_mod_entry*)mi->mods_addr;
        for(int i = 0; i < mi->mods_count; i++) {
            const struct multiboot_mod_entry* mod = modules + i;

            trace("\tmod[%d] %s: %p-%p (%d bytes)",
                  i,
                  mod->str ? mod->str : "",
                  mod->start,
                  mod->end,
                  mod->end - mod->start);

            struct initrd_file* file = kmalloc(sizeof(struct initrd_file));
            bzero(file, sizeof(struct initrd_file));
            strlcpy(file->name, mod->str ? mod->str : "", sizeof(file->name));
            file->size = (unsigned)(mod->end - mod->start);
            file->data = kmalloc(file->size);
            memcpy(file->data, mod->start, file->size);
            list_append(initrd, file, node);
        }
    }
}

#define RUN_TEST(fn) \
    do { \
        trace("Running test %s", #fn); \
        void fn(); \
        fn(); \
    } while(0)


static void run_tests()
{
    //RUN_TEST(test_vmm);
    //RUN_TEST(test_kmalloc);
    //RUN_TEST(test_bitset);
    //RUN_TEST(test_list);
    //RUN_TEST(test_scheduler);
    //RUN_TEST(test_locks);
}

void kmain(const struct multiboot_info* init_multiboot_info)
{
    trace("*** Booted ***");

    /*
     * Multiboot data can be loaded in kernel heap area
     * We solve this problem by relocating multiboot data in
     * lower conventional memory. This will limit our modules
     * and debug symbols to < 600Kb, but this will do for now
     */
    struct multiboot_info multiboot_info = {0};
    multiboot_init(&multiboot_info, init_multiboot_info);
    multiboot_dump(&multiboot_info);

    // Init kernel heap
    kmalloc_init();

    // Load symbols
    load_symbols(&multiboot_info);
    
    // GDT
    gdt_init();
    gdt_iomap_set(DEBUG_PORT, 0);

    // IDT
    idt_init();
    idt_flush();
    idt_install(14, pf_handler, true);
    idt_install(13, gpf_handler, true);

    // Physical memory manager
    pmm_init(&multiboot_info);

    // initrd
    struct initrd initrd;
    load_initrd(&initrd, &multiboot_info);

    /*
     * Reserve currently used memory
     * Low memory: 0x00000000 - 0x000FFFFF
     * kernel code and data section
     * initial kernel heap
     * TODO: Free conventional memory after we load multiboot modules
     */
    for(uint32_t page = 0; page < 0xFFFFF; page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    for(uint32_t page = TRUNCATE(KERNEL_START, PAGE_SIZE); 
        page < KERNEL_END; 
        page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);
    for(uint32_t page = TRUNCATE(heap_info.heap_start, PAGE_SIZE); 
        page < heap_info.heap_start + heap_info.heap_size; 
        page += PAGE_SIZE) {

        if(pmm_exists(page) && !pmm_reserved(page))
            pmm_reserve(page);
    }

    // Virtual memory manager
    vmm_init();

    // PIC
    pic_init();

    // System timer
    timer_init();

    // Syscall handlers
    syscall_init();

    // IPC System
    ipc_init();

    // Run tests
    run_tests();
    reboot();
}




