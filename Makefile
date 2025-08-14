# [Cygnus] - Makefile (boot.s w katalogu głównym)

CC      = i686-elf-gcc

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -std=gnu99 -Iinc -Isrc
LDFLAGS = -T linker.ld -nostdlib

# C-sources
SRC = \
    src/kernel.c \
    src/disk.c \
    src/fat32.c \
    src/fat32_alloc.c \
    src/serial.c \
    src/io.c \
    src/string.c \
    src/std.c

# Twój start w root:
ASM_S = boot.s

OBJ  = $(SRC:.c=.o) $(ASM_S:.s=.o)

KERNEL_BIN = kernel.elf
ISO_NAME   = cygnus.iso

all: $(ISO_NAME)

$(ISO_NAME): $(KERNEL_BIN)
	@mkdir -p iso/boot/grub
	cp $(KERNEL_BIN) iso/boot/kernel.elf
	@echo 'set timeout=0'                  >  iso/boot/grub/grub.cfg
	@echo 'set default=0'                  >> iso/boot/grub/grub.cfg
	@echo 'menuentry "Cygnus" {'           >> iso/boot/grub/grub.cfg
	@echo '  multiboot /boot/kernel.elf'   >> iso/boot/grub/grub.cfg
	@echo '  boot'                         >> iso/boot/grub/grub.cfg
	@echo '}'                              >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_NAME) iso

$(KERNEL_BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) -lgcc

# C → o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# GAS .s → o (root/boot.s → boot.o)
%.o: %.s
	$(CC) -c $< -o $@

clean:
	rm -rf $(OBJ) $(KERNEL_BIN) $(ISO_NAME) iso

.PHONY: all clean