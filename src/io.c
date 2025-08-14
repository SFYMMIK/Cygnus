/*
 * [Cygnus] - [src/io.c]
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
#include "io.h"

/* Wybór dysku: dla primary master ustawiamy 0xE0 | (bity 24..27 LBA).
 * Dla primary slave byłoby 0xF0 | (...), ale na razie obsługujemy master. */
static inline void ata_select_drive_lba28(uint32_t lba) {
    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();
}

/* Czekamy aż BSY=0.
 * (Możemy dodać soft-timeout, ale na start zostawiamy prostą wersję.) */
static inline void ata_wait_not_bsy(void) {
    while (inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_SR_BSY) { }
}

/* Po BSY=0 czekamy na DRQ=1 (dane gotowe) albo błąd. */
static inline int ata_wait_drq_or_err(void) {
    while (1) {
        uint8_t st = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return -1;
        if (st & ATA_SR_DF)  return -2;
        if (st & ATA_SR_DRQ) return 0;
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_RDY)) {
            /* dalej czekamy na DRQ, ale to dobry znak */
        }
    }
}

/* Odczyt 1 sektora 512 B z LBA (PIO) */
int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    /* 1) wybieramy dysk i LBA[24..27] */
    ata_select_drive_lba28(lba);

    /* 2) ustawiamy licznik sektorów = 1 */
    outb(ATA_PRIMARY_IO + ATA_REG_SECCNT, 1);

    /* 3) ustawiamy LBA[0..23] */
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));

    /* 4) komenda READ SECTORS */
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    /* 5) czekamy na BSY=0 i DRQ=1 */
    ata_wait_not_bsy();
    if (ata_wait_drq_or_err() != 0) return -1;

    /* 6) odbieramy 256 słów (512 bajtów) z portu DATA */
    for (int i = 0; i < CYG_SECTOR_SIZE / 2; i++) {
        uint16_t w = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
        buffer[i*2 + 0] = (uint8_t)(w & 0xFF);
        buffer[i*2 + 1] = (uint8_t)(w >> 8);
    }

    /* 7) króciutkie odczekanie */
    io_wait();
    return 0;
}

/* Zapis 1 sektora 512 B do LBA (PIO) */
int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    /* 1) wybór dysku i LBA */
    ata_select_drive_lba28(lba);

    /* 2) licznik sektorów = 1 */
    outb(ATA_PRIMARY_IO + ATA_REG_SECCNT, 1);

    /* 3) LBA[0..23] */
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));

    /* 4) komenda WRITE SECTORS */
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    /* 5) czekamy aż kontroler poprosi o dane (DRQ=1) */
    ata_wait_not_bsy();
    if (ata_wait_drq_or_err() != 0) return -1;

    /* 6) wysyłamy 256 słów (512 bajtów) do portu DATA */
    for (int i = 0; i < CYG_SECTOR_SIZE / 2; i++) {
        uint16_t w = (uint16_t)buffer[i*2 + 0] | ((uint16_t)buffer[i*2 + 1] << 8);
        outw(ATA_PRIMARY_IO + ATA_REG_DATA, w);
    }

    /* 7) króciutkie odczekanie i ewentualne sprawdzenie statusu */
    io_wait();

    /* 8) dla bezpieczeństwa zflushujmy cache (opcjonalnie po serii zapisów) */
    return ata_flush_cache();
}

/* Odczyt wielu sektorów (prosto: wywołujemy single-sector w pętli).
 * Jeżeli chcemy kiedyś wydajniej: możemy użyć komend multiblokowych lub DMA. */
int ata_read_n(uint32_t lba, uint32_t count, void* buffer) {
    uint8_t* p = (uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (ata_read_sector(lba + i, p + i * CYG_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Flush cache (E7h). Niektóre emulatory i tak przyjmą OK, ale wyślijmy,
 * żeby być poprawni. */
int ata_flush_cache(void) {
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_not_bsy();
    /* sprawdzamy błędy */
    uint8_t st = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (st & (ATA_SR_ERR | ATA_SR_DF)) return -1;
    return 0;
}