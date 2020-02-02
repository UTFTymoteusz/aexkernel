CC = gcc
AS = nasm

MKDIR = mkdir -p

BIN  := bin/
BOOT := boot/
DEP_DEST := $(BIN)dep/
OBJ_DEST := $(BIN)obj/

CFILES   := $(shell find . -type f -name '*.c')
HFILES   := $(shell find . -type f -name '*.h')
ASMFILES := $(shell find . -type f -name '*.asm')
PSFFILES := $(shell find . -type f -name '*.psf')
OBJS     := $(patsubst %.o, $(OBJ_DEST)%.o, $(CFILES:.c=.o) $(ASMFILES:.asm=.o) $(PSFFILES:.psf=.o))

OBJS := $(OBJS) ../lai/bin/liblai.a

ISO  = $(BIN)grubiso/
SYS  = $(ISO)sys/

ARCH = arch/x64/

GFLAGS = -O2 -Wall -Wextra -nostdlib -pipe

INCLUDES := -I. -I$(ARCH) -Iinclude/ -Iinclude/libc/ -I../lai/include/

CCFLAGS := $(GFLAGS) \
	-lgcc \
	-std=gnu99 \
	-ffreestanding \
	-masm=intel \
	-mcmodel=kernel \
	-mno-red-zone \
	-fno-pic \
	-fno-stack-protector \
	-fno-omit-frame-pointer \
	$(INCLUDES)

ASFLAGS := -felf64

LDFLAGS := $(GFLAGS) \
	-ffreestanding \
	-z max-page-size=0x1000 \
	-no-pie

-include $(shell find $(DEP_DEST) -type f -name *.d)

all: $(OBJS)
	@$(CC) $(OBJS) $(LDFLAGS) -T linker.ld -o $(SYS)aexkrnl.elf

clean:
	rm -rf $(DEP_DEST)	
	rm -rf $(OBJ_DEST)	

$(OBJ_DEST)%.o : %.c
	@$(MKDIR) ${@D}
	@$(MKDIR) $(dir $(DEP_DEST)$*)
	$(CC) $(CCFLAGS) -c $< -o $@ -MMD -MT $@ -MF $(DEP_DEST)$*.d

$(OBJ_DEST)%.o : %.asm
	@$(MKDIR) ${@D}
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DEST)%.o : %.psf
	@$(MKDIR) ${@D}
	@objcopy -B i386:x86-64 -O elf64-x86-64 -I binary $< $@

iso:
	@grub-mkrescue -o $(BIN)aex.iso $(ISO) 2> /dev/null

qemu:
	qemu-system-x86_64 -machine type=q35 -smp 2 -m 32M -hda "$(BIN)aexa.vdi" -hdb "$(BIN)aexb.vdi" -cdrom $(BIN)aex.iso