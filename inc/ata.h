/*
 * [Cygnus] - [inc/ata.h]
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
#ifndef CYGNUS_ATA_H
#define CYGNUS_ATA_H

#include <stdint.h>
#include <stddef.h>
#include "../src/io.h"   /* korzystamy z istniejących ata_read_sector/ata_write_sector */

/* Rozmiar sektora — przyjmujemy 512 B. */
#ifndef CYG_SECTOR_SIZE
#define CYG_SECTOR_SIZE 512
#endif

/* Odczyt jednego sektora z dysku (wrapper).
 * disk_id: numer dysku (na teraz ignorujemy i zakładamy 0)
 * lba:     numer sektora LBA
 * buf:     bufor wyjściowy o rozmiarze >= 512 bajtów
 */
static inline int ata_lba_read(uint8_t disk_id, uint32_t lba, void* buf) {
    (void)disk_id; /* na razie mamy tylko primary master */
    return ata_read_sector(lba, (uint8_t*)buf);
}

/* Zapis jednego sektora (wrapper). Analogicznie jak wyżej. */
static inline int ata_lba_write(uint8_t disk_id, uint32_t lba, const void* buf) {
    (void)disk_id;
    return ata_write_sector(lba, (const uint8_t*)buf);
}

/* Odczyt wielu sektorów z rzędu (prosta pętla). */
static inline int ata_lba_read_n(uint8_t disk_id, uint32_t lba, uint32_t count, void* buf) {
    uint8_t* p = (uint8_t*)buf;
    for (uint32_t i = 0; i < count; i++) {
        if (ata_lba_read(disk_id, lba + i, p + i * CYG_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Zapis wielu sektorów. */
static inline int ata_lba_write_n(uint8_t disk_id, uint32_t lba, uint32_t count, const void* buf) {
    const uint8_t* p = (const uint8_t*)buf;
    for (uint32_t i = 0; i < count; i++) {
        if (ata_lba_write(disk_id, lba + i, p + i * CYG_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

#endif /* CYGNUS_ATA_H */