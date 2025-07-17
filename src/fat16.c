/*
 * [Cygnus] - [src/fat16.c]
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
#include "fat16.h"
#include "std.h"
#include <string.h>

static fat16_entry_t fat16_entries[FAT16_MAX_ENTRIES];
static int fat16_fd_count = 0, fat16_dir_count = 0;
static fat16_fd_t fat16_fds[16];
static fat16_dir_t fat16_dirs[16];

static int find_free_entry() {
    for (int i = 1; i < FAT16_MAX_ENTRIES; i++)
        if (fat16_entries[i].type == FAT16_FREE)
            return i;
    return -1;
}

static int find_in_dir(int parent, const char *name) {
    int c = fat16_entries[parent].first_child;
    while (c != -1) {
        if (!strcmp(fat16_entries[c].name, name))
            return c;
        c = fat16_entries[c].next_sibling;
    }
    return -1;
}

static int lookup(const char *path) {
    if (!strcmp(path, "/")) return 0;
    int cur = 0;
    char buf[FAT16_MAX_NAME], *p = (char*)path;
    while (*p == '/') p++;
    while (*p) {
        char *slash = strchr(p, '/');
        int len = slash ? (slash-p) : strlen(p);
        strncpy(buf, p, len); buf[len] = 0;
        int idx = find_in_dir(cur, buf);
        if (idx == -1) return -1;
        cur = idx;
        if (!slash) break;
        p = slash+1;
        while (*p == '/') p++;
    }
    return cur;
}

void fat16_init(void) {
    memset(fat16_entries, 0, sizeof(fat16_entries));
    fat16_entries[0].type = FAT16_DIR;
    strcpy(fat16_entries[0].name, "/");
    fat16_entries[0].parent = -1;
    fat16_entries[0].first_child = -1;
    fat16_entries[0].next_sibling = -1;
    for (int i=1; i<FAT16_MAX_ENTRIES; ++i)
        fat16_entries[i].type = FAT16_FREE;
}

int fat16_mkdir(const char *path) {
    int p = 0;
    char name[FAT16_MAX_NAME], *slash = strrchr(path, '/');
    if (slash) {
        if (slash == path) p = 0, strcpy(name, slash+1);
        else {
            strncpy(name, slash+1, FAT16_MAX_NAME);
            name[FAT16_MAX_NAME-1] = 0;
            char dir[FAT16_MAX_NAME];
            strncpy(dir, path, slash-path); dir[slash-path]=0;
            p = lookup(dir);
        }
    } else strcpy(name, path);
    if (p < 0 || find_in_dir(p, name) != -1) return -1;
    int i = find_free_entry(); if (i < 0) return -1;
    fat16_entries[i].type = FAT16_DIR;
    strcpy(fat16_entries[i].name, name);
    fat16_entries[i].parent = p;
    fat16_entries[i].first_child = -1;
    fat16_entries[i].next_sibling = fat16_entries[p].first_child;
    fat16_entries[p].first_child = i;
    return 0;
}

int fat16_open(const char *path, int flags) {
    int i = lookup(path);
    if (i == -1) {
        if (flags == 1) {
            // create
            int p = 0;
            char name[FAT16_MAX_NAME], *slash = strrchr(path, '/');
            if (slash) {
                if (slash == path) p = 0, strcpy(name, slash+1);
                else {
                    strncpy(name, slash+1, FAT16_MAX_NAME); name[FAT16_MAX_NAME-1]=0;
                    char dir[FAT16_MAX_NAME];
                    strncpy(dir, path, slash-path); dir[slash-path]=0;
                    p = lookup(dir);
                }
            } else strcpy(name, path);
            if (p < 0) return -1;
            i = find_free_entry(); if (i < 0) return -1;
            fat16_entries[i].type = FAT16_FILE;
            strcpy(fat16_entries[i].name, name);
            fat16_entries[i].parent = p;
            fat16_entries[i].first_child = -1;
            fat16_entries[i].next_sibling = fat16_entries[p].first_child;
            fat16_entries[p].first_child = i;
            fat16_entries[i].content_size = 0;
        } else return -1;
    }
    if (fat16_entries[i].type != FAT16_FILE) return -1;
    fat16_fds[fat16_fd_count].idx = i;
    fat16_fds[fat16_fd_count].pos = 0;
    return fat16_fd_count++;
}
int fat16_read(int fd, void *buf, int size) {
    if (fd < 0 || fd >= fat16_fd_count) return -1;
    int i = fat16_fds[fd].idx;
    int rem = fat16_entries[i].content_size - fat16_fds[fd].pos;
    if (rem <= 0) return 0;
    if (size > rem) size = rem;
    memcpy(buf, fat16_entries[i].content + fat16_fds[fd].pos, size);
    fat16_fds[fd].pos += size;
    return size;
}
int fat16_write(int fd, const void *buf, int size) {
    if (fd < 0 || fd >= fat16_fd_count) return -1;
    int i = fat16_fds[fd].idx;
    int pos = fat16_fds[fd].pos;
    if (pos + size > FAT16_MAX_FILESIZE) size = FAT16_MAX_FILESIZE - pos;
    memcpy(fat16_entries[i].content + pos, buf, size);
    fat16_fds[fd].pos += size;
    if (fat16_fds[fd].pos > fat16_entries[i].content_size)
        fat16_entries[i].content_size = fat16_fds[fd].pos;
    return size;
}
int fat16_close(int fd) { return 0; }
int fat16_unlink(const char *path) {
    int i = lookup(path); if (i <= 0) return -1;
    fat16_entries[i].type = FAT16_FREE;
    return 0;
}
int fat16_opendir(const char *path) {
    int i = lookup(path); if (i < 0 || fat16_entries[i].type != FAT16_DIR) return -1;
    fat16_dirs[fat16_dir_count].idx = i;
    fat16_dirs[fat16_dir_count].child = fat16_entries[i].first_child;
    return fat16_dir_count++;
}
int fat16_readdir(int dh, char *name, int *is_dir) {
    if (dh < 0 || dh >= fat16_dir_count) return -1;
    int c = fat16_dirs[dh].child;
    if (c == -1) return -1;
    strcpy(name, fat16_entries[c].name);
    *is_dir = (fat16_entries[c].type == FAT16_DIR);
    fat16_dirs[dh].child = fat16_entries[c].next_sibling;
    return 0;
}
int fat16_closedir(int dh) { return 0; }