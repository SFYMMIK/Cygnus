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
#include <stdint.h>

#define ATA_PRIMARY_IO  0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    // Wait for drive ready
    outb(ATA_PRIMARY_CTRL, 0x00);
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)(lba));
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + 7, 0x20);

    // Poll for BSY and DRQ
    for (int i = 0; i < 4; i++) inb(ATA_PRIMARY_IO + 7);
    while (!(inb(ATA_PRIMARY_IO + 7) & 0x08));
    for (int i = 0; i < 256; i++) {
        uint16_t data;
        __asm__ volatile ("inw %1, %0" : "=a"(data) : "Nd"(ATA_PRIMARY_IO));
        buffer[i*2] = data & 0xFF;
        buffer[i*2+1] = (data >> 8) & 0xFF;
    }
    return 0;
}
