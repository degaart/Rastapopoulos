.SUFFIXES:
.PHONY: all clean

include ../../config.mk

CFLAGS += -I ../../common -I ../runtime

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj obj/init.elf

obj:
	@ mkdir -p obj

-include obj/Depends.mk

obj/init.elf: $(OBJS) $(ASM_OBJS) ../../common/obj/common.a ../runtime/obj/runtime.a
	@ echo "[LD] $@"
	@ $(CC) \
		-T ../userland.ld \
		-o $@ \
		-Wl,-Map,$@.map \
		$(LDFLAGS) \
		$^ \
		../runtime/obj/runtime.a \
		../../common/obj/common.a \
		-lgcc

obj/Depends.mk:
	@ CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		../../common/makedepend.sh $(SRCS)

obj/%.c.o:
	@ echo "[CC] $*.c"
	@ $(CC) -c -o $@ $(CFLAGS) $*.c
	@ $(CC) -c -S -o $@.S $(CFLAGS) $*.c

obj/%.asm.o:
	@ echo "[AS] $*.c"
	@ $(AS) -f elf32 -o $@ $*.asm

clean:
	@ rm -r obj/*

