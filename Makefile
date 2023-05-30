#// SPDX-License-Identifier: GPL-2.0-or-later
#/*
# * aVisor Hypervisor
# *
# * A Tiny Hypervisor for IoT Development
# *
# * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
# */

GNU_TOOLS ?= aarch64-linux-gnu
GNU_GCC = $(GNU_TOOLS)-gcc
GNU_LD = $(GNU_TOOLS)-ld
GNU_OBJCOPY = $(GNU_TOOLS)-objcopy
GNU_OBJDUMP = $(GNU_TOOLS)-objdump

INC_DIR = include
C_OPS = -Wall -nostdlib -nostartfiles -ffreestanding -I$(INC_DIR) -mgeneral-regs-only
ASM_OPS = -I$(INC_DIR)

BUILD_DIR = build
OUTDIR = bin

SRC_DIR += hypervisor/arch/aarch64
SRC_DIR += hypervisor/boards/raspi
SRC_DIR += hypervisor/common
SRC_DIR += hypervisor/fs
SRC_DIR += hypervisor/emulator/raspi

HV_DIR = hypervisor

.PHONY: all
all: kernel8.img

.PHONY: qemu
qemu: qemu.img

C_FILES = $(wildcard *.c $(foreach fd, $(SRC_DIR), $(fd)/*.c))
ASM_FILES =$(wildcard *.S $(foreach fd, $(SRC_DIR), $(fd)/*.S))

$(BUILD_DIR)/%_c.o: %.c
	mkdir -p $(dir $@)
	$(GNU_GCC) $(C_OPS) -MMD -c $< -o $@

$(BUILD_DIR)/%_s.o: %.S
	mkdir -p $(dir $@)
	$(GNU_GCC) $(ASM_OPS) -MMD -c $< -o $@

OBJ_FILES = $(C_FILES:%.c=$(BUILD_DIR)/%_c.o)
OBJ_FILES += $(ASM_FILES:%.S=$(BUILD_DIR)/%_s.o)

DEP_FILES = $(OBJ_FILES:%.o=%.d)
-include $(DEP_FILES)

LINKER_FILE = $(HV_DIR)/boards/raspi/linker.ld
LINKER_FILE_QEMU = $(HV_DIR)/boards/raspi/linker_qemu.ld

kernel8.img: $(LINKER_FILE) $(OBJ_FILES)
	$(GNU_LD) -T $(LINKER_FILE) -o $(BUILD_DIR)/kernel8.elf $(OBJ_FILES)
	$(GNU_OBJCOPY) $(BUILD_DIR)/kernel8.elf -O binary $(BUILD_DIR)/kernel8.img
	mkdir -p ./bin
	cp $(BUILD_DIR)/kernel8.img $(OUTDIR)/kernel8.img

qemu.img: $(LINKER_FILE_QEMU) $(OBJ_FILES)
	$(GNU_LD) -T $(LINKER_FILE_QEMU) -o $(BUILD_DIR)/kernel8.elf $(OBJ_FILES)
	$(GNU_OBJCOPY) $(BUILD_DIR)/kernel8.elf -O binary $(BUILD_DIR)/kernel8.img
	mkdir -p ./bin
	cp $(BUILD_DIR)/kernel8.img $(OUTDIR)/kernel8.img
	$(GNU_OBJDUMP) -D $(BUILD_DIR)/kernel8.elf > $(BUILD_DIR)/kernel8.list

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(OUTDIR)

.PHONY: echoes
echoes:
	@echo "C_FILES: $(C_FILES)"
	@echo "ASM_FILES: $(ASM_FILES)"
	@echo "OBJ_FILES: $(OBJ_FILES)"
	@echo "SRC_DIR: $(SRC_DIR)"
