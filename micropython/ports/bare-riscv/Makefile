# Include the core environment definitions; this will set $(TOP).
include ../../py/mkenv.mk

# Include py core make definitions.
include $(TOP)/py/py.mk

# Set makefile-level MicroPython feature configurations.
MICROPY_ROM_TEXT_COMPRESSION ?= 0

# Define toolchain and other tools.
CROSS_COMPILE ?= riscv64-unknown-elf-

API_DIR:= ./sm_api/

# Set CFLAGS.
CFLAGS += -I. -I$(TOP) -I$(BUILD) -I$(API_DIR)
CFLAGS += -Wall -Werror -std=gnu11 -nostdlib
CFLAGS += -march=rv64g -mcmodel=medany -mabi=lp64 -fno-common -fno-tree-loop-distribute-patterns -nostartfiles -nostdlib -static 
CSUPEROPT = -Os # save some code space for performance-critical code

# Select debugging or optimisation build.
DEBUG=0
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -ggdb3
else
CFLAGS += -Os -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections
endif

# Set linker flags.
LDFLAGS += -T enclave.ld
#LDFLAGS += -T enclave.ld --gc-sections

# Define the required source files.
SRC_C += lib.c main.c system.c
SRC_C += \
  ed25519/add_scalar.c \
  ed25519/fe.c \
  ed25519/ge.c \
  ed25519/key_exchange.c \
  ed25519/keypair.c \
  ed25519/sc.c \
  ed25519/sha512.c \
  ed25519/sign.c \
  ed25519/verify.c \

# Define the required object files.
OBJ += $(PY_CORE_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))

OBJ += $(BUILD)/entry.o
$(BUILD)/entry.o: enclave_entry.S
	$(Q)$(CC) -c $(CFLAGS) $(LDFLAGS) -o $@ $^

OBJ += $(BUILD)/setjmp.o
$(BUILD)/setjmp.o: setjmp.S
	$(Q)$(CC) -c $(CFLAGS) $(LDFLAGS) -o $@ $^

OBJ += $(BUILD)/longjmp.o
$(BUILD)/longjmp.o: longjmp.S
	$(Q)$(CC) -c $(CFLAGS) $(LDFLAGS) -o $@ $^

OBJ += $(BUILD)/enclave_stack.o
$(BUILD)/enclave_stack.o: enclave_stack.S
	$(Q)$(CC) -c $(CFLAGS) $(LDFLAGS) -o $@ $^

# Define the top-level target, the main firmware.
all: $(BUILD)/firmware.bin

$(BUILD)/firmware.elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^
	$(Q)$(SIZE) $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(ECHO) "Create $@"
	$(Q)$(OBJCOPY) -O binary $^ $@

ELFS := $(shell find $(BUILD) -name '*.elf')
ELFS_PREF := $(addprefix $(BUILD)/, $(ELFS))
DISASS = $(ELFS:.elf=.disa.out)
DISASS_SOURCES = $(ELFS:.elf=.src.out)
OBJDUMP = $(CROSS_COMPILE)objdump

%.disa.out : %.elf
	$(Q)$(OBJDUMP) -D $^ > $@

%.src.out : %.elf
	$(Q)$(OBJDUMP) -S $^ > $@

.PHONY: disassemble-all
disassemble-all:$(DISASS)

.PHONY: source-all
source-all:$(DISASS_SOURCES)

# Include remaining core make rules.
include $(TOP)/py/mkrules.mk
