/*
 * [Cygnus] - [src/fat32.h]
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
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int (*fat32_read_sectors_fn)(void *dev, uint64_t lba, uint32_t count,
                                     void *buf);

void *fat32_malloc(size_t sz);
void fat32_free(void *p);

// log debug który chyba działa (wyjebane w to czy działa czy nie)
void fat32_log(const char *fmt, ...);

#pragma pack(push, 1)
typedef struct {
  uint8_t jmp[3];
  char oem[8];
  uint16_t bytes_per_sector; // 512 dla tego sterownika
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t num_fats;
  uint16_t root_entry_count; // 0 dla FAT32
  uint16_t total_sectors_16;
  uint8_t media;
  uint16_t fat_size16; // 0 dla FAT32
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t total_sectors32;

  // FAT32 EXTENDED
  uint32_t fat_size32; // sektory na jeden FAT
  uint16_t ext_flags;
  uint16_t fs_version;
  uint32_t root_cluster; // zazwyczaj 2
  uint16_t fsinfo;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  uint8_t drive_number;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fat_type[8];
  uint8_t boot_code[420];
  uint16_t boot_signature55AA;
} fat32_bpb_t;

typedef struct {
  uint32_t lead_sig; // 0x41615252
  uint8_t reserved1[480];
  uint32_t struct_sig; // 0x61417272
  uint32_t free_count;
  uint32_t next_free;
  uint8_t reserved2[12];
  uint32_t trail_sig; // 0xAA550000 (mały endian na dysku)
} fat32_fsinfo_t;

typedef struct {
  uint8_t name[11]; // 8 + 3, pojebana matematyka
  uint8_t attr;     // 0x10 dir, 0x20 plik, 0x08 vol, 0x0F LFN
  uint8_t ntres;
  uint8_t crtTimeTenth;
  uint16_t crtTime;
  uint16_t crtDate;
  uint16_t lstAccDate;
  uint16_t firstClusterHigh;
  uint16_t wrtTime;
  uint16_t wrtDate;
  uint16_t firstClusterLow;
  uint32_t fileSize;
} fat32_dirent_t;

// Długie entry nazw plików (attr=0x0F)
typedef struct {
  uint8_t order;
  uint16_t name1[5];
  uint8_t attr;
  uint8_t type;
  uint8_t checksum;
  uint16_t name2[6];
  uint16_t firstCluster;
  uint16_t name3[2];
} fat32_lfn_t;
#pragma pack(pop)

enum {
  FAT32_ATTR_READ_ONLY = 0x01,
  FAT32_ATTR_HIDDEN = 0x02,
  FAT32_ATTR_SYSTEM = 0x04,
  FAT32_ATTR_VOLUME_ID = 0x08,
  FAT32_ATTR_DIRECTORY = 0x10,
  FAT32_ATTR_ARCHIVE = 0x20,
  FAT32_ATTR_LFN = 0x0F,
};

typedef struct {
  void *dev;
  fat32_read_sectors_fn read;
  fat32_bpb_t bpb;

  uint32_t bytes_per_sector;
  uint32_t sectors_per_cluster;
  uint32_t first_data_sector;
  uint32_t fat_start_lba;
  uint32_t root_dir_first_cluster; // numer klastra (cluster)
  uint32_t total_clusters;
} fat32_volume_t;

typedef struct {
  uint32_t start_cluster;
  uint32_t size_bytes;
  uint32_t pos;
  bool is_dir;
  fat32_volume_t *vol;
  // prosty buffer dla jednego klastra (cluster)
  uint8_t *cluster_buf;
  uint32_t cluster_buf_num;
} fat32_file_t;

typedef struct {
  char name[256]; // UTF-8 przekonwertowane z LFN lub 8.3
  bool is_dir;
  uint32_t size;
  uint32_t first_cluster;
} fat32_dirent_info_t;

// API (nie no rozkurwi mnie od wewnątrz jak będę musiał to naprawiać(teraz też
// rozpierdala))
int fat32_mount(fat32_volume_t *vol, void *dev, fat32_read_sectors_fn read_fn);
int fat32_open(fat32_volume_t *vol, const char *path, fat32_file_t **out);
int fat32_read(fat32_file_t *f, void *buf, uint32_t nbytes, uint32_t *out_read);
void fat32_close(fat32_file_t *f);

int fat32_readdir_first(fat32_volume_t *vol, uint32_t dir_cluster,
                        fat32_file_t **dir_handle);
int fat32_readdir_next(fat32_file_t *dir_handle, fat32_dirent_info_t *out);
void fat32_readdir_close(fat32_file_t *dir_handle);

/* wspomagacze */
uint32_t fat32_cluster_to_lba(const fat32_volume_t *vol, uint32_t clus);
int fat32_next_cluster(const fat32_volume_t *vol, uint32_t current,
                       uint32_t *next_out);
bool fat32_is_eoc(uint32_t clus);