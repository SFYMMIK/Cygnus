CC = gcc
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -no-pie -fno-stack-protector -Iinc

KERNEL_SRC = src/shell.c src/std.c src/fat16.c
SBIN_SRC = src/sbin/ls.c src/sbin/cat.c      # Add more commands here: src/sbin/edit.c src/sbin/mkdir.c ...

OBJS = $(KERNEL_SRC:.c=.o) $(SBIN_SRC:.c=.o)

.PHONY: all clean iso run

all: kernel.elf

kernel.elf: $(OBJS)
	$(CC) $(CFLAGS) -nostdlib -T linker.ld -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o src/*.o src/sbin/*.o kernel.elf kernel.iso
	rm -rf iso

iso: kernel.elf
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/kernel.elf
	echo 'set timeout=0\ndefault=0\nmenuentry "MinimalOS" {\n multiboot2 /boot/kernel.elf\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o kernel.iso iso

run: iso
	qemu-system-i386 -cdrom kernel.iso