# Cygnus — Build & Test Guide

This is a concise, end-to-end README for building, creating a test disk, and running Cygnus in QEMU. It also lists what changed from the previous version.

---

## Prerequisites

Install a cross compiler and the tools used to build the ISO and run QEMU.

**Debian/Ubuntu**
```bash
sudo apt-get install i686-elf-gcc xorriso mtools grub-pc-bin dosfstools qemu-system-i386 util-linux
```

**Arch**
```bash
yay -S i686-elf-gcc-bin xorriso mtools grub dosfstools qemu-system-i386 util-linux
```

**Fedora**
```bash
sudo dnf install i686-elf-gcc xorriso mtools grub2-pc-modules dosfstools qemu-system-i386 util-linux
```

> If `grub-mkrescue` later complains about `mformat`, it means `mtools` isn’t installed.

---

## Project layout (important bits)

```
boot.s                 # Multiboot1 header + entry symbol 'start' -> calls kmain
linker.ld              # ENTRY(start), .multiboot placed before .text
Makefile               # builds kernel.elf and cygnus.iso

inc/
  serial.h, disk.h, ata.h, string.h, std.h, ...

src/
  kernel.c             # UART console + mini shell (ls/cat), FAT32 mount
  disk.c               # MBR scan + adapter for FAT32
  fat32.c              # FAT32 (read-only)
  fat32_alloc.c        # tiny arena allocator (fat32_malloc/free)
  serial.c             # COM1 UART
  io.c, string.c, std.c
```

---

## Build the kernel & ISO

```bash
make clean
make
```

Artifacts:
- `kernel.elf`
- `cygnus.iso`

---

## Create a test HDD image (MBR + FAT32)

You have two options:

### Option A — use the provided script (recommended)
If you saved the helper script as `test.sh`:

```bash
chmod +x test.sh
./test.sh             # creates disk.img (64 MiB), FAT32, adds README.TXT
```

The script auto-handles both new and old `sfdisk` syntax and detaches the loop device at the end.

### Option B — use your `test.sh`
Even if your `test.sh` prints messages in Polish, **the final QEMU command it prints still works** — just copy/paste it. (It creates `disk.img`, partitions it, formats FAT32, drops a `README.TXT`, then shows how to run QEMU.)

---

## Run in QEMU

Use the HDD image as **IDE disk 0** and boot the ISO via GRUB:

```bash
qemu-system-i386   -drive file=disk.img,format=raw,if=ide,index=0   -cdrom cygnus.iso -boot d -serial stdio
```

Why both?  
- The ISO provides GRUB + your kernel.
- The HDD provides a real MBR + FAT32 partition that the kernel scans and mounts.

On boot you should see the UART shell prompt on your terminal.

---

## Using the built-in shell

Available commands (type them into the QEMU window/terminal when using `-serial stdio`):

```
help               # show commands
ls [PATH]          # list directory (default: /)
cat PATH           # print file (e.g. /README.TXT)
reboot             # soft reset
halt               # halt CPU
```

Quick test:
```text
> ls
> cat /README.TXT
```

---

## What changed since the previous version

**Kernel / UX**
- Added a tiny UART shell on COM1 (115200 8N1) with `ls`, `cat`, `reboot`, and `halt`.
- No immediate halt after boot; you can explore the filesystem.

**Drivers / FS**
- FAT32 driver wired to a simple per-driver arena allocator (`fat32_alloc.c`) providing `fat32_malloc/free`.
- BPB field names synchronized with your `fat32.h` (`fat_size32`, `total_sectors32`, `total_sectors_16`).
- Replaced libc `printf/snprintf` with your freestanding `kprintf/ksnprintf` from `std.h`.
- Directory listing prints without `%10u` / `%u` pitfalls to keep it freestanding-safe.

**Disk / Boot**
- `disk.c` exposes `mbr_scan(...)` returning partition info (type, `lba_start`, length).
- `boot.s` (GAS/AT&T) now provides a valid Multiboot v1 header and `start` entry; calls `kmain`.
- `linker.ld`: `ENTRY(start)`, `.multiboot` section placed before `.text`; load base at 1 MiB.

**Build system**
- Makefile builds `kernel.elf` and `cygnus.iso` (GRUB), and supports `boot.s` located at project root.
- Clear error messages if required tools are missing.
- Notes on installing `xorriso`, `mtools`, `grub-pc-bin`.

**Utilities**
- Added a robust `make_fat32_disk.sh` (with fallback for older `sfdisk`) to create an MBR+FAT32 HDD image and put `README.TXT` on it.

**Minor**
- `inc/string.h` should declare `strncmp` (warning if missing).
- Tiny style fix suggestion in `src/string.c` to avoid a misleading-indentation warning.

---

## Troubleshooting

- **`grub-mkrescue: mformat failed`** → install `mtools`.
- **No files under `/`** → ensure you boot with a separate `disk.img` as IDE index 0 (see QEMU command).
- **Stuck after boot** → you’re in the shell; type `help`. Use `halt` to stop, `reboot` to reset.
- **Serial output missing** → ensure you launch QEMU with `-serial stdio`.
- **`strncmp` implicit declaration** → add its prototype to your `inc/string.h` (already shown above).
- **Old `sfdisk`** → use the provided script (`test.sh`) which falls back to the old syntax automatically.

---

## License

All new/edited files provided here use **EUPL v1.2** as noted in file headers.

---

## One-shot demo (copy/paste)

```bash
# Build
make clean && make

# Create HDD (64 MiB) with FAT32 + README.TXT
./make_fat32_disk.sh

# Run (GRUB from ISO, HDD as IDE0)
qemu-system-i386   -drive file=disk.img,format=raw,if=ide,index=0   -cdrom cygnus.iso -boot d -serial stdio

# In the shell:
# > ls
# > cat README.TXT (or cat /README.TXT if in a diffrent dir which you will be able to do after i add it)
# > halt
```