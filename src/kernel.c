/*
 * [Cygnus] - [src/kernel.c]
 *
 * Copyright (C) [2025] [Szymon Grajner]
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the European Union Public Licence (EUPL) V.1.2 or - as
 * soon as they will be approved by the European Commission - subsequent
 * versions of the EUPL (the "Licence").
 *
 * You may not use this work except in compliance with the Licence.
 * You may obtain a copy of the Licence at:
 * https://joinup.ec.europa.eu/software/page/eupl/licence-eupl
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 */
#include "../inc/serial.h"
#include "../inc/std.h"
#include "../inc/disk.h"
#include "fat32.h"

/* Globalnie: urządzenie blokowe i wolumin FAT32 */
static fat32_volume_t g_vol;
static disk_dev_t     g_dev;

/* ======== Pomocnicze ======== */

static inline void halt_forever(void) {
    for (;;) { __asm__ __volatile__("hlt"); }
}

/* proste porównania bez zależności od strncmp */
static int streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}
static int starts_with(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}
static const char* skip_ws(const char* s) {
    while (*s==' ' || *s=='\t') s++;
    return s;
}

/* prosta linia z UART (obsługa backspace), zawsze kończymy NUL-em */
static void serial_getline(char* out, int cap) {
    int n = 0;
    for (;;) {
        char c = serial_read();
        if (c == '\r' || c == '\n') { serial_write("\r\n"); break; }
        if ((c == 8 || c == 127)) {
            if (n > 0) { n--; serial_write("\b \b"); }
            continue;
        }
        if (n+1 < cap) { out[n++] = c; serial_write_char(c); }
    }
    out[n] = 0;
}

/* wypis jednego wpisu katalogu – unikamy %10u/%u */
static void print_dirent(const fat32_dirent_info_t* inf) {
    kprintf("%c ", inf->is_dir ? 'd' : '-');
    kprintf("%d  ", (int)inf->size);
    kprintf("%s\n", inf->name);
}

/* ls dla ścieżki (albo root) */
static void fs_ls(const char* path) {
    if (!path || !*path) path = "/";

    fat32_file_t* f = NULL;
    int rc = fat32_open(&g_vol, path, &f);
    if (rc) {
        kprintf("[ERR] ls: nie znaleziono: %s (kod=%d)\n", path, rc);
        return;
    }
    if (!f->is_dir) {
        kprintf("[ERR] ls: to nie katalog: %s\n", path);
        fat32_close(f);
        return;
    }

    fat32_file_t* it = NULL;
    rc = fat32_readdir_first(&g_vol, f->start_cluster, &it);
    fat32_close(f);
    if (rc) {
        kprintf("[ERR] readdir_first kod=%d\n", rc);
        return;
    }

    while (1) {
        fat32_dirent_info_t inf;
        rc = fat32_readdir_next(it, &inf);
        if (rc == 1) break;       /* koniec */
        if (rc != 0) { kprintf("[ERR] readdir_next=%d\n", rc); break; }
        print_dirent(&inf);
    }
    fat32_readdir_close(it);
}

/* cat pliku (tekstowo; binarki też pokaże jako znaki) */
static void fs_cat(const char* path) {
    if (!path || !*path) { kprintf("Użycie: cat /ŚCIEŻKA\n"); return; }

    fat32_file_t* f = NULL;
    int rc = fat32_open(&g_vol, path, &f);
    if (rc) { kprintf("[ERR] cat: nie znaleziono: %s (kod=%d)\n", path, rc); return; }
    if (f->is_dir) { kprintf("[ERR] cat: to katalog: %s\n", path); fat32_close(f); return; }

    uint8_t buf[512];
    uint32_t got = 0;
    do {
        rc = fat32_read(f, buf, sizeof(buf), &got);
        if (got) {
            for (uint32_t i = 0; i < got; i++) serial_write_char((char)buf[i]);
        }
    } while (rc == 0 && got > 0);

    serial_write("\r\n");
    fat32_close(f);
}

/* Montujemy pierwszą partycję FAT32 (0x0B/0x0C) z dysku 0 */
static int fs_init(void) {
    mbr_partition_t parts[4];

    kprintf("[INIT] Skanujemy MBR (dysk 0)...\n");
    if (mbr_scan(0, parts) != 0) {
        kprintf("[ERR] Brak MBR albo błędna sygnatura.\n");
        return -1;
    }

    for (int i = 0; i < 4; i++) {
        uint8_t t = parts[i].type;
        if (t == 0x0B || t == 0x0C) {
            kprintf("[OK] Znaleźliśmy FAT32 na partycji %d (LBA start=%u, rozmiar=%u sektorów)\n",
                    i + 1, (unsigned)parts[i].lba_start, (unsigned)parts[i].sectors_total);

            g_dev.disk_id = 0;
            g_dev.base_lba = parts[i].lba_start;

            int rc = fat32_mount(&g_vol, &g_dev, fat32_read_from_disk);
            if (rc == 0) {
                kprintf("[OK] FAT32 zamontowany poprawnie.\n");
                return 0;
            } else {
                kprintf("[ERR] Mount FAT32 nie powiódł się (kod=%d)\n", rc);
                return -2;
            }
        }
    }
    kprintf("[WARN] Nie znaleźliśmy partycji FAT32.\n");
    return -3;
}

/* Minimalna powłoka na UART */
static void shell_loop(void) {
    char line[128];
    kprintf("\n[TTY] Prosta powłoka. Komendy: help | ls [PATH] | cat PATH | reboot | halt\n");
    for (;;) {
        serial_write("> ");
        serial_getline(line, sizeof(line));
        const char* s = skip_ws(line);
        if (*s == 0) continue;

        if (streq(s, "help")) {
            kprintf("help\nls [PATH]\ncat PATH\nreboot\nhalt\n");
            continue;
        }
        if (streq(s, "halt")) {
            kprintf("[HALT] Zatrzymujemy CPU.\n");
            halt_forever();
        }
        if (streq(s, "reboot")) {
            kprintf("[REBOOT]\n");
            /* reset przez KBC (0x64 ← 0xFE) */
            __asm__ __volatile__(
                "mov $0xFE, %%al\n\t"
                "out %%al, $0x64\n\t"
                :
                : : "al"
            );
            halt_forever();
        }
        if (streq(s, "ls")) { fs_ls("/"); continue; }
        if (starts_with(s, "ls "))   { fs_ls(skip_ws(s+2)); continue; }
        if (starts_with(s, "cat "))  { fs_cat(skip_ws(s+3)); continue; }

        kprintf("[ERR] Nie znam: %s\n", s);
    }
}

/* ======== Wejście jądra ======== */
void kmain(void) {
    serial_init(COM1_BASE);
    kprintf("\n=== Cygnus kernel ===\n");

    int disks = disk_enumerate();
    kprintf("[INIT] Dyski widoczne: %d\n", disks);

    if (fs_init() == 0) {
        kprintf("[FS] Zawartość katalogu głównego:\n");
        fs_ls("/");
    }

    /* zamiast natychmiastowego HALT — powłoka */
    shell_loop();
}