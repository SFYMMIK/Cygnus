/*
 * [Cygnus] - [src/io.h]
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
#ifndef CYGNUS_IO_H
#define CYGNUS_IO_H

#include <stdint.h>
#include <stddef.h>

/* ===== Podstawowe funkcje dostępu do portów I/O (x86) ===== */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* 400 ns opóźnienia dla niektórych kontrolerów ATA — klasyczny hack:
 * odczyt z „portu opóźniającego” 0x80 kilka razy. */
static inline void io_wait(void) {
    outb(0x80, 0);
    outb(0x80, 0);
    outb(0x80, 0);
    outb(0x80, 0);
}

/* ===== Stałe dla kanału ATA (primary) ===== */

#define ATA_PRIMARY_IO   0x1F0  /* Data + reg. I/O */
#define ATA_PRIMARY_CTL  0x3F6  /* Control/AltStatus */

/* Rejestry (offsety od ATA_PRIMARY_IO) */
#define ATA_REG_DATA     0x00   /* 16-bit data */
#define ATA_REG_ERROR    0x01   /* read:  Error */
#define ATA_REG_FEATURES 0x01   /* write: Features */
#define ATA_REG_SECCNT   0x02   /* Sector Count */
#define ATA_REG_LBA0     0x03   /* LBA low */
#define ATA_REG_LBA1     0x04   /* LBA mid */
#define ATA_REG_LBA2     0x05   /* LBA high */
#define ATA_REG_HDDEVSEL 0x06   /* Drive/Head */
#define ATA_REG_STATUS   0x07   /* read:  Status */
#define ATA_REG_COMMAND  0x07   /* write: Command */

/* Bity rejestru STATUS */
#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_SRV  0x10
#define ATA_SR_DF   0x20
#define ATA_SR_RDY  0x40
#define ATA_SR_BSY  0x80

/* Komendy ATA PIO */
#define ATA_CMD_READ_SECTORS   0x20  /* PIO read (with retry) */
#define ATA_CMD_WRITE_SECTORS  0x30  /* PIO write (with retry) */
#define ATA_CMD_CACHE_FLUSH    0xE7

/* Rozmiar sektora (w bajtach) */
#ifndef CYG_SECTOR_SIZE
#define CYG_SECTOR_SIZE 512
#endif

/* ===== API ATA PIO (primary master, LBA28) =====
 * LBA: 28-bit (maks ~128 GiB); bufor musi mieć >=512 B na sektor.
 */

/* Odczyt 1 sektora (512 B) z LBA do bufora. Zwraca 0 gdy OK. */
int ata_read_sector(uint32_t lba, uint8_t* buffer);

/* Zapis 1 sektora (512 B) z bufora do LBA. Zwraca 0 gdy OK. */
int ata_write_sector(uint32_t lba, const uint8_t* buffer);

/* Odczyt wielu sektorów w pętli. Zwraca 0 gdy OK. */
int ata_read_n(uint32_t lba, uint32_t count, void* buffer);

/* Flush cache dysku (dobry zwyczaj po serii zapisów). */
int ata_flush_cache(void);

#endif /* CYGNUS_IO_H */
