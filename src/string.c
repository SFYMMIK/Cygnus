/*
 * [Cygnus] - [src/string.c]
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

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}
void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *dp = d; const uint8_t *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) a++, b++;
    return *(const unsigned char*)a - *(const unsigned char*)b;
}
size_t strlen(const char *s) {
    size_t n = 0; while (*s++) n++; return n;
}
char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}
char *strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return c == 0 ? (char*)s : NULL;
}
char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do { if (*s == (char)c) last = s; } while (*s++);
    return (char*)last;
}
char *strtok(char *str, const char *delim) {
    static char *save;
    if (str) save = str;
    if (!save) return NULL;
    char *s = save + strspn(save, delim);
    if (!*s) { save = NULL; return NULL; }
    char *e = s + strcspn(s, delim);
    if (*e) *e++ = 0;
    save = e;
    return s;
}
size_t strspn(const char *s, const char *accept) {
    const char *p; size_t n = 0;
    while (*s) {
        for (p = accept; *p && *p != *s; p++);
        if (!*p) break;
        s++; n++;
    }
    return n;
}
size_t strcspn(const char *s, const char *reject) {
    const char *p; size_t n = 0;
    while (*s) {
        for (p = reject; *p && *p != *s; p++);
        if (*p) break;
        s++; n++;
    }
    return n;
}
