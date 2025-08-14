/*
 * [Cygnus] - [src/disk.c]
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
#include "../inc/ata.h"
#include "../inc/disk.h"
#include <stdint.h>

/* Zakładamy na razie jeden dysk (disk_id=0). Później możemy dodać enumerację. */
static int g_disk_count = 1;

int disk_enumerate(void) { g_disk_count = 1; return g_disk_count; }
int disk_count(void) { return g_disk_count; }

/* Czytamy 'count' sektorów zaczynając od LBA (32-bit). */
int disk_read_sectors(int disk_id, uint32_t lba, uint32_t count, void* buf) {
    return ata_lba_read_n((uint8_t)disk_id, lba, count, buf);
}

/* Skanujemy MBR (LBA0) i przepisujemy 4 wpisy do mbr_partition_t.
 * Zwraca 0 gdy OK, <0 przy błędzie/nieprawidłowej sygnaturze.
 * Uwaga: struktury mbr_t i mbr_partition_t pochodzą z inc/disk.h.
 */
int mbr_scan(int disk_id, mbr_partition_t out_parts[4]) {
    uint8_t sector[CYG_SECTOR_SIZE];
    if (disk_read_sectors(disk_id, 0, 1, sector) != 0) return -1;

    const mbr_t* m = (const mbr_t*)sector;
    if (m->signature != 0xAA55) return -2;

    /* Kopiujemy surowe wpisy z MBR do naszego formatu nagłówkowego */
    for (int i = 0; i < 4; i++) {
        out_parts[i].boot_flag     = m->partitions[i].boot_flag;
        out_parts[i].chs_start[0]  = m->partitions[i].chs_start[0];
        out_parts[i].chs_start[1]  = m->partitions[i].chs_start[1];
        out_parts[i].chs_start[2]  = m->partitions[i].chs_start[2];
        out_parts[i].type          = m->partitions[i].type;
        out_parts[i].chs_end[0]    = m->partitions[i].chs_end[0];
        out_parts[i].chs_end[1]    = m->partitions[i].chs_end[1];
        out_parts[i].chs_end[2]    = m->partitions[i].chs_end[2];
        out_parts[i].lba_start     = m->partitions[i].lba_start;
        out_parts[i].sectors_total = m->partitions[i].sectors_total;
    }
    return 0;
}

/* Adapter dla FAT32: przesuwamy LBA o base_lba partycji i czytamy.
 * MUSI mieć uint64_t lba (zgodnie z fat32_read_sectors_fn w fat32.h).
 * Nasza dolna warstwa obsługuje 32-bit LBA (LBA28), więc pilnujemy zakresów.
 */
int fat32_read_from_disk(void* dev, uint64_t lba, uint32_t count, void* buf) {
    const disk_dev_t* d = (const disk_dev_t*)dev;

    /* policz fizyczne LBA w obrębie całego dysku */
    uint64_t phys_lba = (uint64_t)d->base_lba + lba;

    /* dopóki mamy LBA28/32-bit, odrzucamy zakres > 0xFFFFFFFF */
    if (phys_lba > 0xFFFFFFFFull) return -1;

    /* czytamy sektor po sektorze (wystarczy na start) */
    uint8_t* p = (uint8_t*)buf;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t cur = phys_lba + i;
        if (cur > 0xFFFFFFFFull) return -2;
        int rc = disk_read_sectors(d->disk_id, (uint32_t)cur, 1, p + i * CYG_SECTOR_SIZE);
        if (rc) return rc;
    }
    return 0;
}