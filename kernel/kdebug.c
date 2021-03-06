#include "kernel.h"
#include "debug.h"
#include "string.h"
#include "io.h"
#include "elf.h"
#include "multiboot.h"
#include "registers.h"
#include "util.h"
#include "kmalloc.h"
#include "locks.h"
#include "pmm.h"

struct debug_sym {
    const char* name;
    uint32_t start;
    uint32_t end;
};

// TODO: Find a way to use dynamic memory here
// For now, just use a static table until we implement kmalloc
static unsigned             _debug_syms_count = 0;
static struct debug_sym*    _debug_syms = NULL;
static char*                _debug_strings = NULL;
static uint64_t             tsc_freq;
static uint64_t             tsc_start;

static void __log_callback(int ch, void* unused)
{
    outb(DEBUG_PORT, ch);
}

void debug_printv(const char* fmt, va_list args)
{
    formatv(__log_callback, NULL, fmt, args);
}

void debug_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    debug_printv(fmt, args);
    va_end(args);
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    // strip path from file
    const char* basename = file + strlen(file);
    while(basename >= file && *basename != '/')
        basename--;
    if(*basename == '/')
        basename++;
    else if(!*basename)
        basename = file;


    enter_critical_section();


    uint64_t ts = rdtsc() - tsc_start;
    if(tsc_freq)
        ts /= tsc_freq;
    else
        ts = 0;
    format(__log_callback, NULL, "%06lld [%s:%d][%s] ", ts, basename, line, func);

    va_list args;
    va_start(args, fmt);
    formatv(__log_callback, NULL, fmt, args);
    va_end(args);

    __log_callback('\n', NULL);

    leave_critical_section();
}

const char* lookup_function(uint32_t address)
{
    const char* result = NULL;
    for(unsigned sym_index = 0; sym_index < _debug_syms_count; sym_index++) {
        if(address >= _debug_syms[sym_index].start && address < _debug_syms[sym_index].end) {
            result = _debug_syms[sym_index].name;
            break;
        }
    }
    return result;
}

void backtrace()
{
    uint32_t* ebp = (uint32_t*)read_ebp();

#if 0
    uint32_t stack_start = (uint32_t) (initial_kernel_stack);
    uint32_t stack_end = (uint32_t) (initial_kernel_stack + 4096);
#else
    uint32_t stack_start = TRUNCATE(read_esp(), PAGE_SIZE);
    uint32_t stack_end = stack_start + PAGE_SIZE;
#endif

    uint32_t data[255];
    unsigned index = 0;
    data[index++] = *(ebp + 1);

    while(1) {
        if((uint32_t)ebp <= stack_start || ((uint32_t)ebp+(sizeof(uint32_t)*2)) >= stack_end)
            break;

        uint32_t* prev_ebp = (uint32_t*) *ebp;
        if((uint32_t)prev_ebp <= stack_start || ((uint32_t)prev_ebp+(sizeof(uint32_t)*2)) >= stack_end)
            break;
        uint32_t prev_eip = *(prev_ebp + 1);

        data[index++] = prev_eip;
        ebp = prev_ebp;
    }
    
    trace("Backtrace:");
    for(unsigned i = 0; i < index; i++) {
#if 0
        const char* name = NULL;
        for(unsigned sym_index = 0; sym_index < _debug_syms_count; sym_index++) {
            if(data[i] >= _debug_syms[sym_index].start && data[i] < _debug_syms[sym_index].end) {
                name = _debug_syms[sym_index].name;
                break;
            }
        }
#endif
        const char* name = lookup_function(data[i]);
        trace("\t0x%X %s", data[i], name ? name : "??");
    }
}

void load_symbols(const struct multiboot_info* multiboot_info)
{
    // All headers
    elf32_shdr_t* hdrs = (elf32_shdr_t*)multiboot_info->sym2.addr;
    trace("Debug headers: %p", hdrs);
   
    // Section name table header
    elf32_shdr_t* shstr_hdr = &hdrs[multiboot_info->sym2.shndx];
    trace("Section header strings: %p", shstr_hdr);

    const char* section_names = (const char*)shstr_hdr->sh_addr;
    trace("Section_names: %p", section_names);

    // Symbols table header
    elf32_shdr_t* sym_hdr = NULL;

    // Symbol names table header
    elf32_shdr_t* strtab_hdr = NULL;
    
    // Find relevant sections
    for(unsigned i = 0; i < multiboot_info->sym2.num; i++) {
        if(hdrs[i].sh_type == SHT_SYMTAB) {
            sym_hdr = &hdrs[i];
        } else if(!strcmp(section_names + hdrs[i].sh_name, ".strtab")) {
            strtab_hdr = &hdrs[i];
        }

        if(sym_hdr && strtab_hdr)
            break;
    }

    // Dump all this
    if(sym_hdr && strtab_hdr) {
        _debug_strings = kmalloc(strtab_hdr->sh_size);
        memcpy(_debug_strings, (void*)strtab_hdr->sh_addr, strtab_hdr->sh_size);

        unsigned sym_count = sym_hdr->sh_size / sizeof(elf32_sym_t);
        _debug_syms = kmalloc(sym_count * sizeof(_debug_syms[0]));

        elf32_sym_t* syms = (elf32_sym_t*)sym_hdr->sh_addr;
        for(unsigned i = 0; i < sym_count; i++) {
            if(ELF32_ST_TYPE(syms[i].st_info) == STT_FUNC || 
               ELF32_ST_TYPE(syms[i].st_info) == STT_OBJECT) {
                _debug_syms[_debug_syms_count].name = _debug_strings + syms[i].st_name;
                _debug_syms[_debug_syms_count].start = syms[i].st_value;
                _debug_syms[_debug_syms_count].end = syms[i].st_value + syms[i].st_size;
                _debug_syms_count++;
            }
        }
    }
}

void __assertion_failed(const char* function, const char* file, int line, const char* expression)
{
    __log(function, file, line, "Assertion failed: %s", expression);
    abort();
}

void kdebug_init()
{
    /*
     * Calibrate tsc
     */
    uint64_t tsc0 = rdtsc();
    io_delay();
    io_delay();
    io_delay();
    io_delay();
    uint64_t tsc1 = rdtsc();
    tsc_freq = tsc1 - tsc0;
    tsc_start = rdtsc();
}


