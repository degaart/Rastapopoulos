/**
 * Virtual memory manager
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VMM_PAGE_PRESENT        0x1
#define VMM_PAGE_WRITABLE       0x2
#define VMM_PAGE_USER           0x4

#define KERNEL_START            0xC0000000
#define KERNEL_END              0xFFBFFFFF

#define USER_START              0x1000
#define USER_END                0xBFFFFFFF

struct pagedir;

void vmm_init();
void vmm_map(void* va, uint32_t pa, uint32_t flags);
void vmm_unmap(void* va);
void vmm_remap(void* va, uint32_t flags);
void vmm_flush_tlb(void* va);
bool vmm_paging_enabled();
void vmm_copy_kernel_mappings(struct pagedir* pagedir); /* copy current kernel mappings into specified pagedir */
void vmm_switch_pagedir(struct pagedir* pagedir); /* VA, but translated internally into physical address */
void vmm_destroy_pagedir(struct pagedir* pagedir);
void vmm_reset_current_pagedir();                       /* Reset all user mappings of current pagetable */

#if 0
struct pagedir* vmm_current_pagedir();
#endif

struct pagedir* vmm_clone_pagedir();
uint32_t vmm_get_physical(void* va); /* Returns 0 if va is not mapped */
uint32_t vmm_get_flags(void* va);

void* vmm_transient_map(uint32_t frame, unsigned flags);
void vmm_transient_unmap(void* address);




