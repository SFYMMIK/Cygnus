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
    uint8_t *p = (uint8_t*)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}
void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *dp = (uint8_t*)d; const uint8_t *sp = (const uint8_t*)s;
    while (n--) *dp++ = *sp++;
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t*)a, *pb = (const uint8_t*)b;
    for (; n; --n, ++pa, ++pb) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
    }
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s; while (*p) ++p; return (size_t)(p - s);
}
char *strcpy(char *d, const char *s) {
    char *r = d; while ((*d++ = *s++)); return r;
}
char *strncpy(char *d, const char *s, size_t n) {
    size_t i=0; for (; i<n && s[i]; ++i) d[i]=s[i];
    for (; i<n; ++i) d[i]=0;
    return d;
}
int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}
int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i=0; i<n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb || ca == 0 || cb == 0) return (int)ca - (int)cb;
    }
    return 0;
}
char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; ++s; }
    return (c == 0) ? (char*)s : NULL;
}

/* Prosty strtok bez wsparcia dla wielu delimiterów – wystarcza na whitespace */
static int is_space(char c){ return c==' ' || c=='\t' || c=='\n' || c=='\r'; }
char *strtok(char *s, const char *delim) {
    static char *save;
    if (!delim || delim[0]==0 || delim[1]!=0) { /* wspieramy pojedynczy delim */ }
    char d = delim ? delim[0] : ' ';
    if (s) save = s;
    if (!save) return NULL;
    while (*save && (is_space(*save) || *save==d)) ++save;
    if (!*save) { save = NULL; return NULL; }
    char *start = save;
    while (*save && !is_space(*save) && *save!=d) ++save;
    if (*save) { *save++ = 0; }
    return start;
}