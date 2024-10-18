# Compiler and flags
CC := riscv64-unknown-elf-gcc
OBJDUMP := riscv64-unknown-elf-objdump
CFLAGS := -march=rv64g -mabi=lp64d -O2 -fno-expensive-optimizations -Wall -Wextra

# Source files and output names
SOURCES := memcpy_shm.c memcpy_shm_opt.c memcpy_shm_simple.c ml_ext.c
OBJECTS := $(SOURCES:.c=.o)
ASMS := $(SOURCES:.c=.asm)

# Default target
all: $(OBJECTS) $(ASMS)

# Generic rule to create .asm from .o
%.asm: %.o
	$(OBJDUMP) -d $< > $@

# Clean target
clean:
	rm -f $(OBJECTS) $(ASMS)

.PHONY: all clean
