CC = gcc
AS = nasm

MKDIR = mkdir -p

BIN  := bin/
BOOT := boot/
OBJ_DEST := $(BIN)obj/

CFILES   := $(shell find . -type f -name '*.c')
ASMFILES := $(shell find . -type f -name '*.asm')
PSFFILES := $(shell find . -type f -name '*.psf')
OBJS     := $(patsubst %.o, $(OBJ_DEST)%.o, $(CFILES:.c=.o) $(ASMFILES:.asm=.o) $(PSFFILES:.psf=.o))
DEPENDS  := $(patsubst %.o, %.d, $(OBJS))

ISO  = $(BIN)grubiso/
SYS  = $(ISO)sys/

ARCH = arch/x64/

GFLAGS = -O2 -Wall -Wextra -nostdlib -pipe

ASFLAGS := -felf64

CCFLAGS := $(GFLAGS) \
	-lgcc \
	-std=gnu99 \
	-ffreestanding \
	-masm=intel \
	-mcmodel=kernel \
	-mno-sse \
	-mno-sse2 \
	-mno-red-zone \
	-fno-pic \
	-fno-stack-protector \
	-I. \
	-I$(ARCH) \
	-Ikernel/libc/ \
	-Ikernel/libk/

LDFLAGS := $(GFLAGS) \
	-ffreestanding \
	-z max-page-size=0x1000 \
	-no-pie

all: $(OBJS)
	@$(CC) $(OBJS) $(LDFLAGS) -T linker.ld -o $(SYS)aexkrnl.elf

-include $(DEPENDS)

$(OBJ_DEST)%.o: %.c
	@$(MKDIR) ${@D}
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJ_DEST)%.o: %.asm
	@$(MKDIR) ${@D}
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DEST)%.o: %.psf
	@objcopy -B i386:x86-64 -O elf64-x86-64 -I binary $< $@

iso:
	@grub-mkrescue -o $(BIN)aex.iso $(ISO) 2> /dev/null