/*
 * [Cygnus] - [src/std.c]
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
#include "../inc/std.h"
#include "../inc/serial.h"
#include <stddef.h>

/* Prosty print na UART (bez formatowania) */
void print(const char *s) { while (*s) { serial_write_char(*s++); } }

/* Wypisanie pojedynczego znaku na UART */
void putchar(char c) { serial_write_char(c); }

/* Bardzo prosty input – czytamy do \n/\r, backspace działa */
void gets(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = serial_read();
        if (c == '\r' || c == '\n') break;
        if (c == 8 || c == 127) {        /* Backspace/DEL */
            if (i > 0) { --i; print("\b \b"); }
            continue;
        }
        buf[i++] = c;
        putchar(c);
    }
    buf[i] = 0;
    print("\r\n");
}

/* --- Pomocnicze: konwersje liczb --- */
static int utoa_base(unsigned long v, unsigned base, char *out, int outsz, int upper) {
    static const char digs_l[] = "0123456789abcdef";
    static const char digs_u[] = "0123456789ABCDEF";
    const char *digs = upper ? digs_u : digs_l;
    char tmp[32];
    int n = 0;
    if (base < 2 || base > 16) return 0;
    if (v == 0) tmp[n++] = '0';
    while (v && n < (int)sizeof(tmp)) { tmp[n++] = digs[v % base]; v /= base; }
    int w = 0;
    while (n && w < outsz-1) out[w++] = tmp[--n];
    out[w] = 0;
    return w;
}
static int itoa10(long v, char *out, int outsz) {
    if (v < 0) {
        if (outsz < 2) return 0;
        *out++ = '-'; outsz--;
        return 1 + utoa_base((unsigned long)(-(v)), 10, out, outsz, 0);
    }
    return utoa_base((unsigned long)v, 10, out, outsz, 0);
}

/* --- rdzeń formatowania --- */
int kvsnprintf(char *dst, int dstsz, const char *fmt, va_list ap) {
    int written = 0;
    if (!dst || dstsz <= 0) return 0;

    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') {
            if (written < dstsz - 1) dst[written] = *p;
            written++;
            continue;
        }
        /* '%' */
        ++p;
        if (*p == 0) break;

        int upper = 0;
        char tmp[64];

        switch (*p) {
            case '%':
                if (written < dstsz - 1) dst[written] = '%';
                written++;
                break;
            case 'c': {
                int c = va_arg(ap, int);
                if (written < dstsz - 1) dst[written] = (char)c;
                written++;
            } break;
            case 's': {
                const char *s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                while (*s) {
                    if (written < dstsz - 1) dst[written] = *s;
                    written++; s++;
                }
            } break;
            case 'd': {
                long v = va_arg(ap, int);
                int n = itoa10(v, tmp, sizeof(tmp));
                for (int i=0;i<n;i++) { if (written < dstsz - 1) dst[written] = tmp[i]; written++; }
            } break;
            case 'u': {
                unsigned long v = va_arg(ap, unsigned int);
                int n = utoa_base(v, 10, tmp, sizeof(tmp), 0);
                for (int i=0;i<n;i++) { if (written < dstsz - 1) dst[written] = tmp[i]; written++; }
            } break;
            case 'x': /* hex lower */
            case 'p': {
                unsigned long v = (unsigned long)va_arg(ap, unsigned long);
                int n = utoa_base(v, 16, tmp, sizeof(tmp), upper);
                for (int i=0;i<n;i++) { if (written < dstsz - 1) dst[written] = tmp[i]; written++; }
            } break;
            case 'X':
                upper = 1;
                /* fallthrough do 'x' */
                p--;
                break;
            default:
                /* nieznany format – wypisujemy dosłownie */
                if (written < dstsz - 1) dst[written] = '%';
                written++;
                if (written < dstsz - 1) dst[written] = *p;
                written++;
                break;
        }
    }

    /* NUL-terminate */
    if (dstsz > 0) {
        if (written >= dstsz) dst[dstsz - 1] = 0;
        else dst[written] = 0;
    }
    return written;
}

int ksnprintf(char *dst, int dstsz, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(dst, dstsz, fmt, ap);
    va_end(ap);
    return n;
}

void kprintf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, (int)sizeof(buf), fmt, ap);
    va_end(ap);
    /* wypis na UART */
    for (int i = 0; i < n && buf[i]; ++i) serial_write_char(buf[i]);
}