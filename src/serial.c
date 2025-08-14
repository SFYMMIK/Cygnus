/*
 * [Cygnus] - [src/serial.c]
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
#include "io.h"        /* inb/outb – u nas nagłówek jest w src/ */
#include <stdint.h>

/* Rejestry względem bazy */
enum {
    REG_DATA      = 0, /* RBR/THR (DLAB=0) */
    REG_IER       = 1, /* Interrupt Enable (DLAB=0) / Divisor High (DLAB=1) */
    REG_LCR       = 3, /* Line Control */
    REG_MCR       = 4, /* Modem Control */
    REG_LSR       = 5, /* Line Status */
    REG_FCR       = 2, /* FIFO Control */
    /* Uwaga: przy DLAB=1, REG_DATA (0) = Divisor Low, REG_IER (1) = Divisor High */
};

/* Bity w LSR */
#define LSR_DATA_READY   0x01
#define LSR_THR_EMPTY    0x20

static uint16_t g_serial_base = COM1_BASE;

void serial_set_base(uint16_t base) { g_serial_base = base; }

void serial_init(uint16_t base) {
    g_serial_base = base;

    /* Wyłączamy przerwania z UART (na razie nie używamy IRQ) */
    outb(g_serial_base + REG_IER, 0x00);

    /* DLAB=1: ustawiamy dzielnik dla 115200 (divisor = 1) */
    outb(g_serial_base + REG_LCR, 0x80);       /* DLAB=1 */
    outb(g_serial_base + REG_DATA, 0x01);      /* DLL (low)  */
    outb(g_serial_base + REG_IER,  0x00);      /* DLM (high) */

    /* 8N1, DLAB=0 */
    outb(g_serial_base + REG_LCR, 0x03);       /* 8 bit, bez parzystości, 1 stop */

    /* Włączamy FIFO, czyścimy RX/TX, threshold 14 bajtów */
    outb(g_serial_base + REG_FCR, 0xC7);

    /* Ustawiamy DTR/RTS; OUT2 niepotrzebne bez IRQ, więc 0x03 wystarczy */
    outb(g_serial_base + REG_MCR, 0x03);

    /* Opcjonalne „przepalenie” – czytamy LSR i RBR, żeby wyczyścić */
    (void)inb(g_serial_base + REG_LSR);
    (void)inb(g_serial_base + REG_DATA);
}

static inline void serial_wait_tx_empty(void) {
    while ((inb(g_serial_base + REG_LSR) & LSR_THR_EMPTY) == 0) { /* spin */ }
}

int serial_can_read(void) {
    return (inb(g_serial_base + REG_LSR) & LSR_DATA_READY) ? 1 : 0;
}

char serial_read(void) {
    while (!serial_can_read()) { /* spin */ }
    return (char)inb(g_serial_base + REG_DATA);
}

void serial_write_char(char c) {
    /* Konwersja LF → CR+LF dla czytelności na terminalach */
    if (c == '\n') {
        serial_wait_tx_empty();
        outb(g_serial_base + REG_DATA, '\r');
    }
    serial_wait_tx_empty();
    outb(g_serial_base + REG_DATA, (uint8_t)c);
}

void serial_write(const char *s) {
    while (*s) {
        serial_write_char(*s++);
    }
}