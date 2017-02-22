#pragma once

#include "multiboot.h"
#include <stdbool.h>

#define PAGE_SIZE 4096

void pmm_init(const struct multiboot_info* multiboot_info);
bool pmm_initialized();
void pmm_reserve(uint32_t page);
bool pmm_reserved(uint32_t page);

