/*
 * [Cygnus] - [src/fat32_alloc.c]
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
#include <stddef.h>
#include <stdint.h>

#ifndef FAT32_ARENA_SIZE
#define FAT32_ARENA_SIZE (128 * 1024) /* 128 KiB na struktury FAT32 */
#endif

/* Wyrównujemy do 16 bajtów dla bezpieczeństwa. */
static uint8_t g_fat32_arena[FAT32_ARENA_SIZE];
static size_t  g_fat32_off = 0;

static inline size_t align_up(size_t x, size_t a) {
    return (x + (a - 1)) & ~(a - 1);
}

/* API oczekiwane przez fat32.c */
void* fat32_malloc(size_t n) {
    if (n == 0) n = 1;
    size_t off = align_up(g_fat32_off, 16);
    size_t end = off + align_up(n, 16);
    if (end > FAT32_ARENA_SIZE) {
        return NULL; /* brak miejsca */
    }
    void* p = (void*)(g_fat32_arena + off);
    g_fat32_off = end;
    return p;
}

void fat32_free(void* p) {
    (void)p; /* no-op – arena jest jednorazowa */
}