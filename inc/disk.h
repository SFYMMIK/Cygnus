/*
 * [Cygnus] - [inc/disk.h]
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
#ifndef CYGNUS_DISK_H
#define CYGNUS_DISK_H

#include <stdint.h>
#include <stdbool.h>

/* Rozmiar sektora (bytes) – trzymamy 512 B */
#ifndef CYG_SECTOR_SIZE
#define CYG_SECTOR_SIZE 512
#endif

/* Pojedynczy wpis partycji z MBR (LBA0) */
typedef struct {
    uint8_t  boot_flag;      /* 0x80 jeżeli aktywna */
    uint8_t  chs_start[3];
    uint8_t  type;           /* 0x0B / 0x0C = FAT32 */
    uint8_t  chs_end[3];
    uint32_t lba_start;      /* początek partycji w sektorach LBA */
    uint32_t sectors_total;  /* długość partycji w sektorach */
} __attribute__((packed)) mbr_partition_t;

/* Cały sektor MBR (512 B) */
typedef struct {
    uint8_t         boot_code[446];
    mbr_partition_t partitions[4];
    uint16_t        signature;   /* 0xAA55 (LE) */
} __attribute__((packed)) mbr_t;

/* Abstrakcja „urządzenia dyskowego” używana przez FAT32.
 * base_lba = offset początku partycji (tak, żeby FAT32 widział LBA=0 jako start partycji).
 */
typedef struct {
    int      disk_id;
    uint32_t base_lba;
} disk_dev_t;

/* API warstwy dyskowej */
int disk_enumerate(void);
int disk_count(void);

/* UJEDNOLICONA SYGNATURA: count jest uint32_t */
int disk_read_sectors(int disk_id, uint32_t lba, uint32_t count, void* buf);

/* Skan MBR – wypełnia 4 wpisy; zwraca 0 gdy OK, <0 gdy błąd/sygnatura != 0xAA55 */
int mbr_scan(int disk_id, mbr_partition_t out_parts[4]);

/* Adapter dla FAT32: MUSI mieć uint64_t lba (jak w fat32_read_sectors_fn) */
int fat32_read_from_disk(void* dev, uint64_t lba, uint32_t count, void* buf);

#endif /* CYGNUS_DISK_H */