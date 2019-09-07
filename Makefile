CC = gcc
AS = nasm

BIN  = bin/
BOOT = boot/

ISO  = $(BIN)grubiso/
SYS  = $(ISO)sys/

ARCH = arch/x64/

GFLAGS = -march=x86-64 -O2 -Wall -Wextra -nostdlib -g 

CCFLAGS := $(GFLAGS) \
	-lgcc \
	-std=gnu99 \
	-ffreestanding \
	-masm=intel \
	-mcmodel=kernel \
	-fno-pic \
	-I. \
	-Iarch/x64/ \
	-Ikernel/libc/ \
	-Ikernel/libk/

LDFLAGS := $(GFLAGS) \
	-ffreestanding \
	-z max-page-size=0x1000 \
	-no-pie

OBJS := $(BIN)arch.o $(BIN)bootstrap.o $(BIN)entry.o $(BIN)kernel.o

all:
	$(AS) -felf64 -g -o $(BIN)arch.o $(ARCH)root.asm
	$(AS) -felf64 -g -o $(BIN)entry.o $(BOOT)entry.asm
	$(AS) -felf64 -g -o $(BIN)bootstrap.o $(ARCH)boot/bootstrap.asm
	$(CC) $(CCFLAGS) -c -o $(BIN)kernel.o main.c
	$(CC) $(LDFLAGS) -T linker.ld -o $(SYS)aexkrnl.elf $(OBJS)
	grub-mkrescue -o $(BIN)aex.iso $(ISO) --xorriso=/home/tymk/xorriso-1.4.6/xorriso/xorriso