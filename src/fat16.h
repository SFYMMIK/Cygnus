/*
 * [Cygnus] - [src/fat16.h]
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
#ifndef FAT16_H
#define FAT16_H

#define FAT16_MAX_NAME 64
#define FAT16_MAX_ENTRIES 128
#define FAT16_MAX_FILESIZE 4096

typedef enum { FAT16_FILE, FAT16_DIR, FAT16_FREE } fat16_type_t;

typedef struct {
    char name[FAT16_MAX_NAME];
    fat16_type_t type;
    int parent;   // Index in entries
    int first_child, next_sibling;
    char content[FAT16_MAX_FILESIZE];
    int content_size;
} fat16_entry_t;

typedef struct {
    int idx;
    int pos;
} fat16_fd_t;

typedef struct {
    int idx;
    int child;
} fat16_dir_t;

void fat16_init(void);

int fat16_open(const char *path, int flags);
int fat16_read(int fd, void *buf, int size);
int fat16_write(int fd, const void *buf, int size);
int fat16_close(int fd);

int fat16_opendir(const char *path);
int fat16_readdir(int dh, char *name, int *is_dir);
int fat16_closedir(int dh);

int fat16_mkdir(const char *path);
int fat16_unlink(const char *path);

#endif