/*
 * [Cygnus] - [src/fat32.c]
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
#include "fat32.h"
#include <stdarg.h>
#include <string.h>
#include "../inc/std.h"   /* ksnprintf/kprintf, putchar/print */

/* ===== Minimalne wartości domyślne ===== */
#ifndef FAT32_STATIC
#define FAT32_STATIC static
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ZERO
#define ZERO(p, sz) memset((p), 0, (sz))
#endif

/* ===== Wewnętrzne narzędzia ===== */

FAT32_STATIC void dlog(const char *fmt, ...) {
#ifdef FAT32_ENABLE_LOG
  va_list ap;
  va_start(ap, fmt);
  char buf[256];
  kvsnprintf(buf, (int)sizeof(buf), fmt, ap);
  va_end(ap);
  kprintf("%s", buf);
#else
  (void)fmt;
#endif
}

FAT32_STATIC uint16_t rd16(const void *p) {
  const uint8_t *b = (const uint8_t*)p;
  return (uint16_t)(b[0] | (b[1] << 8));
}
FAT32_STATIC uint32_t rd32(const void *p) {
  const uint8_t *b = (const uint8_t*)p;
  return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}

/* ===== Główne funkcje pomocnicze ===== */

uint32_t fat32_cluster_to_lba(const fat32_volume_t *vol, uint32_t clus) {
  uint32_t first_sector_of_cluster =
      (clus - 2) * vol->sectors_per_cluster + vol->first_data_sector;
  return first_sector_of_cluster;
}

bool fat32_is_eoc(uint32_t clus) { return (clus >= 0x0FFFFFF8U); }

int fat32_next_cluster(const fat32_volume_t *vol, uint32_t current,
                       uint32_t *next_out) {
  /* Każdy wpis FAT32 ma 32 bity (górne 4 zarezerwowane) */
  uint32_t fat_offset_bytes = current * 4;
  uint32_t sector =
      vol->fat_start_lba + (fat_offset_bytes / vol->bytes_per_sector);
  uint32_t offset = fat_offset_bytes % vol->bytes_per_sector;

  uint8_t *buf = (uint8_t *)fat32_malloc(vol->bytes_per_sector);
  if (!buf)
    return -1;
  int rc = vol->read(vol->dev, sector, 1, buf);
  if (rc) {
    fat32_free(buf);
    return -2;
  }

  uint32_t val = rd32(buf + offset) & 0x0FFFFFFF;
  fat32_free(buf);
  *next_out = val;
  return 0;
}

/* ===== Montowanie ===== */

int fat32_mount(fat32_volume_t *vol, void *dev, fat32_read_sectors_fn read_fn) {
  ZERO(vol, sizeof(*vol));
  vol->dev = dev;
  vol->read = read_fn;

  /* Odczytujemy sektor rozruchowy partycji (LBA 0 względem partycji) */
  uint8_t *sector = (uint8_t *)fat32_malloc(512);
  if (!sector)
    return -1;
  if (read_fn(dev, 0, 1, sector)) {
    fat32_free(sector);
    return -2;
  }

  fat32_bpb_t *bpb = (fat32_bpb_t *)sector;

  if (bpb->boot_signature55AA != 0xAA55) {
    fat32_free(sector);
    return -3;
  }
  if (bpb->bytes_per_sector != 512) {
    fat32_free(sector);
    return -4;
  }
  if (bpb->num_fats < 1) {
    fat32_free(sector);
    return -5;
  }
  if (bpb->fat_size32 == 0) {  /* UWAGA: bez podkreślenia */
    fat32_free(sector);
    return -6; /* musi być FAT32 */
  }

  memcpy(&vol->bpb, bpb, sizeof(fat32_bpb_t));
  fat32_free(sector);

  vol->bytes_per_sector       = vol->bpb.bytes_per_sector;
  vol->sectors_per_cluster    = vol->bpb.sectors_per_cluster;
  vol->fat_start_lba          = vol->bpb.reserved_sectors;
  vol->root_dir_first_cluster = vol->bpb.root_cluster;

  uint32_t total_sectors = vol->bpb.total_sectors32
                           ? vol->bpb.total_sectors32
                           : vol->bpb.total_sectors_16; /* <-- poprawione */

  uint32_t fats_area       = vol->bpb.num_fats * vol->bpb.fat_size32;
  uint32_t data_start_lba  = vol->bpb.reserved_sectors + fats_area;
  vol->first_data_sector   = data_start_lba;

  uint32_t data_sectors    = total_sectors - data_start_lba;
  vol->total_clusters      = data_sectors / vol->sectors_per_cluster;

  /* sanity check liczby klastrów dla FAT32 */
  if (vol->total_clusters < 65525)
    return -7; /* za mało → to nie FAT32 */

  return 0;
}

/* ===== Przechodzenie po katalogach i obsługa LFN ===== */

FAT32_STATIC void trim_spaces(char *s) {
  /* usuwamy spacje z końca */
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ')
    s[--n] = 0;
}

FAT32_STATIC void entry_83_to_name(const fat32_dirent_t *de, char out[256]) {
  char name[9];
  char ext[4];
  memcpy(name, de->name, 8);
  name[8] = 0;
  memcpy(ext, de->name + 8, 3);
  ext[3] = 0;
  trim_spaces(name);
  trim_spaces(ext);
  if (ext[0])
    ksnprintf(out, 256, "%s.%s", name, ext);
  else
    ksnprintf(out, 256, "%s", name);
}

FAT32_STATIC int utf16le_to_utf8(const uint16_t *in, size_t in_len, char *out,
                                 size_t out_sz) {
  /* naiwny konwerter tylko dla BMP (LFN używa UCS-2 bez surogatów) */
  size_t oi = 0;
  for (size_t i = 0; i < in_len; i++) {
    uint16_t ch = in[i];
    if (ch == 0x0000 || ch == 0xFFFF)
      break;
    if (ch < 0x80) {
      if (oi + 1 >= out_sz) return -1;
      out[oi++] = (char)ch;
    } else if (ch < 0x800) {
      if (oi + 2 >= out_sz) return -1;
      out[oi++] = 0xC0 | (ch >> 6);
      out[oi++] = 0x80 | (ch & 0x3F);
    } else {
      if (oi + 3 >= out_sz) return -1;
      out[oi++] = 0xE0 | (ch >> 12);
      out[oi++] = 0x80 | ((ch >> 6) & 0x3F);
      out[oi++] = 0x80 | (ch & 0x3F);
    }
  }
  if (oi < out_sz) out[oi] = 0;
  return 0;
}

FAT32_STATIC uint8_t lfn_checksum(const uint8_t name83[11]) {
  uint8_t sum = 0;
  for (int i = 0; i < 11; i++)
    sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + name83[i];
  return sum;
}

typedef struct {
  char assembled[260];
  uint8_t needed;
  uint8_t seen_mask; /* bit i dla fragmentu i (liczone od 1) */
  uint8_t checksum;
  bool valid;
} lfn_accum_t;

FAT32_STATIC void lfn_reset(lfn_accum_t *acc) { ZERO(acc, sizeof(*acc)); }

FAT32_STATIC void lfn_feed(lfn_accum_t *acc, const fat32_lfn_t *lfn) {
  uint8_t ord = lfn->order & 0x1F;
  if (lfn->order & 0x40) { /* ostatni fragment */
    acc->needed = ord;
    acc->seen_mask = 0;
    acc->checksum = lfn->checksum;
    acc->valid = true;
    ZERO(acc->assembled, sizeof(acc->assembled));
  }
  if (!acc->valid || ord == 0 || ord > acc->needed) return;

  /* zapisujemy 13-znakowy fragment jako UTF-16LE */
  uint16_t tmp[13];
  for (int i = 0; i < 5; i++)  tmp[i]      = lfn->name1[i];
  for (int i = 0; i < 6; i++)  tmp[5 + i]  = lfn->name2[i];
  for (int i = 0; i < 2; i++)  tmp[11 + i] = lfn->name3[i];

  char utf8[40];
  ZERO(utf8, sizeof(utf8));
  utf16le_to_utf8(tmp, 13, utf8, sizeof(utf8));

  /* dodajemy na początek, żeby końcowa kolejność była poprawna */
  char cur[260];
  strncpy(cur, acc->assembled, sizeof(cur));
  ksnprintf(acc->assembled, sizeof(acc->assembled), "%s%s", utf8, cur);

  acc->seen_mask |= (1U << (ord - 1));
}

FAT32_STATIC void build_dirent_info(const fat32_dirent_t *de,
                                    const lfn_accum_t *acc,
                                    fat32_dirent_info_t *out) {
  ZERO(out, sizeof(*out));
  out->is_dir = (de->attr & FAT32_ATTR_DIRECTORY) != 0;
  out->size = de->fileSize;
  out->first_cluster =
      ((uint32_t)de->firstClusterHigh << 16) | de->firstClusterLow;

  if (acc && acc->valid && acc->assembled[0] && acc->needed) {
    /* szybka weryfikacja checksumy */
    if (lfn_checksum(de->name) == acc->checksum) {
      strncpy(out->name, acc->assembled, sizeof(out->name) - 1);
      return;
    }
  }
  /* w razie niepowodzenia — format 8.3 */
  entry_83_to_name(de, out->name);
}

FAT32_STATIC int read_entire_cluster(const fat32_volume_t *vol, uint32_t clus,
                                     uint8_t *buf) {
  uint32_t lba = fat32_cluster_to_lba(vol, clus);
  return vol->read(vol->dev, lba, vol->sectors_per_cluster, buf);
}

FAT32_STATIC int iterate_dir(fat32_volume_t *vol, uint32_t start_cluster,
                             int (*cb)(const fat32_dirent_t *,
                                       const fat32_dirent_info_t *, void *),
                             void *opaque) {
  const uint32_t bytes_per_cluster =
      vol->bytes_per_sector * vol->sectors_per_cluster;
  uint8_t *clusbuf = (uint8_t *)fat32_malloc(bytes_per_cluster);
  if (!clusbuf)
    return -1;

  uint32_t clus = start_cluster;
  while (!fat32_is_eoc(clus)) {
    if (read_entire_cluster(vol, clus, clusbuf)) {
      fat32_free(clusbuf);
      return -2;
    }

    lfn_accum_t lacc;
    lfn_reset(&lacc);

    for (uint32_t off = 0; off < bytes_per_cluster;
         off += sizeof(fat32_dirent_t)) {
      fat32_dirent_t *de = (fat32_dirent_t *)(clusbuf + off);
      if (de->name[0] == 0x00)
        break; /* koniec katalogu */
      if (de->name[0] == 0xE5) {
        lfn_reset(&lacc);
        continue;
      } /* wpis usunięty */
      if (de->attr == FAT32_ATTR_LFN) {
        lfn_feed(&lacc, (fat32_lfn_t *)de);
        continue;
      }
      /* pomijamy etykiety woluminu */
      if (de->attr & FAT32_ATTR_VOLUME_ID) {
        lfn_reset(&lacc);
        continue;
      }

      fat32_dirent_info_t info;
      build_dirent_info(de, &lacc, &info);
      lfn_reset(&lacc);
      int rc = cb(de, &info, opaque);
      if (rc != 0) {
        fat32_free(clusbuf);
        return rc;
      }
    }
    uint32_t next;
    if (fat32_next_cluster(vol, clus, &next)) {
      fat32_free(clusbuf);
      return -3;
    }
    if (next == 0x0FFFFFF7) { /* uszkodzony klaster */
      fat32_free(clusbuf);
      return -4;
    }
    clus = next;
  }

  fat32_free(clusbuf);
  return 0;
}

/* ===== Rozwiązywanie ścieżek ===== */

typedef struct {
  const char *name; /* komponent ścieżki */
  size_t len;
} path_comp_t;

FAT32_STATIC const char *skip_slashes(const char *s) {
  while (*s == '/' || *s == '\\') s++;
  return s;
}

FAT32_STATIC const char *next_comp(const char *s, path_comp_t *out) {
  s = skip_slashes(s);
  if (!*s) {
    out->name = NULL;
    out->len = 0;
    return s;
  }
  const char *e = s;
  while (*e && *e != '/' && *e != '\\') e++;
  out->name = s;
  out->len = (size_t)(e - s);
  return e;
}

typedef struct {
  const char *target;
  size_t target_len;
  fat32_dirent_info_t found;
  const fat32_dirent_t *found_raw;
  bool matched;
} find_ctx_t;

FAT32_STATIC int match_cb(const fat32_dirent_t *raw,
                          const fat32_dirent_info_t *info, void *opaque) {
  find_ctx_t *ctx = (find_ctx_t *)opaque;
  if (!ctx->target || !ctx->target_len)
    return 0;
  /* porównanie bez rozróżniania wielkości liter */
  char name[256];
  strncpy(name, info->name, sizeof(name));
  for (char *p = name; *p; ++p)
    if (*p >= 'a' && *p <= 'z') *p -= 32;
  char tgt[256];
  ZERO(tgt, sizeof(tgt));
  strncpy(tgt, ctx->target, MIN(sizeof(tgt) - 1, ctx->target_len));
  for (char *p = tgt; *p; ++p)
    if (*p >= 'a' && *p <= 'z') *p -= 32;

  if (strncmp(name, tgt, 255) == 0) {
    ctx->found = *info;
    ctx->found_raw = raw;
    ctx->matched = true;
    return 1; /* zatrzymujemy iterację */
  }
  return 0;
}

FAT32_STATIC int resolve_path_to_entry(fat32_volume_t *vol, const char *path,
                                       fat32_dirent_info_t *out) {
  /* zaczynamy od katalogu głównego */
  uint32_t cur = vol->root_dir_first_cluster;

  path = skip_slashes(path);
  if (!*path) {
    /* katalog główny */
    ZERO(out, sizeof(*out));
    out->is_dir = true;
    out->size = 0;
    out->first_cluster = cur;
    strcpy(out->name, "/");
    return 0;
  }

  path_comp_t comp;
  while (1) {
    const char *next = next_comp(path, &comp);
    find_ctx_t ctx = {.target = comp.name, .target_len = comp.len};
    int rc = iterate_dir(vol, cur, match_cb, &ctx);
    if (rc < 0) return rc;
    if (!ctx.matched) return -10; /* nie znaleziono */

    /* Czy to ostatni komponent? */
    next = skip_slashes(next);
    if (!*next) {
      *out = ctx.found;
      return 0;
    }

    /* Musi być katalogiem, aby kontynuować */
    if (!ctx.found.is_dir) return -11; /* to nie katalog */
    cur = ctx.found.first_cluster;
    path = next;
  }
}

/* ===== Otwieranie / czytanie ===== */

int fat32_open(fat32_volume_t *vol, const char *path, fat32_file_t **out) {
  fat32_dirent_info_t inf;
  int rc = resolve_path_to_entry(vol, path, &inf);
  if (rc) return rc;

  fat32_file_t *f = (fat32_file_t *)fat32_malloc(sizeof(*f));
  if (!f) return -1;
  ZERO(f, sizeof(*f));
  f->vol = vol;
  f->start_cluster = inf.first_cluster;
  f->size_bytes = inf.size;
  f->is_dir = inf.is_dir;
  f->pos = 0;
  f->cluster_buf =
      (uint8_t *)fat32_malloc(vol->sectors_per_cluster * vol->bytes_per_sector);
  if (!f->cluster_buf) {
    fat32_free(f);
    return -2;
  }
  f->cluster_buf_num = (uint32_t)-1;
  *out = f;
  return 0;
}

FAT32_STATIC int cluster_index_and_offset(const fat32_volume_t *vol,
                                          uint32_t pos, uint32_t *cluster_index,
                                          uint32_t *offset_in_cluster) {
  const uint32_t csz = vol->sectors_per_cluster * vol->bytes_per_sector;
  *cluster_index = pos / csz;
  *offset_in_cluster = pos % csz;
  return 0;
}

FAT32_STATIC int get_cluster_at_index(const fat32_volume_t *vol,
                                      uint32_t start_cluster, uint32_t idx,
                                      uint32_t *out_clus) {
  uint32_t c = start_cluster;
  for (uint32_t i = 0; i < idx; i++) {
    if (fat32_is_eoc(c)) return -1;
    uint32_t n;
    if (fat32_next_cluster(vol, c, &n)) return -2;
    c = n;
  }
  *out_clus = c;
  return 0;
}

int fat32_read(fat32_file_t *f, void *buf, uint32_t nbytes,
               uint32_t *out_read) {
  if (f->is_dir) return -12; /* czytanie bajtów z katalogu nieobsługiwane */

  uint32_t remain = (f->pos < f->size_bytes) ? (f->size_bytes - f->pos) : 0;
  uint32_t toread = MIN(remain, nbytes);
  uint8_t *dst = (uint8_t *)buf;

  const uint32_t csz = f->vol->sectors_per_cluster * f->vol->bytes_per_sector;

  uint32_t done = 0;
  while (done < toread) {
    uint32_t cidx, off;
    cluster_index_and_offset(f->vol, f->pos, &cidx, &off);

    /* Upewniamy się, że bufor klastra jest załadowany dla tego cidx */
    if (f->cluster_buf_num != cidx) {
      uint32_t clus;
      if (get_cluster_at_index(f->vol, f->start_cluster, cidx, &clus)) break;
      if (read_entire_cluster(f->vol, clus, f->cluster_buf)) break;
      f->cluster_buf_num = cidx;
    }

    uint32_t chunk = MIN(csz - off, toread - done);
    memcpy(dst + done, f->cluster_buf + off, chunk);
    done += chunk;
    f->pos += chunk;
  }

  if (out_read) *out_read = done;
  return (done == toread) ? 0 : -13;
}

void fat32_close(fat32_file_t *f) {
  if (!f) return;
  if (f->cluster_buf) fat32_free(f->cluster_buf);
  fat32_free(f);
}

/* ===== Readdir (iteracja wpisów w katalogu) ===== */

typedef struct {
  /* używamy fat32_file_t, ale traktujemy to jako iterator katalogu */
  fat32_file_t base;
  /* stan iteracji */
  uint32_t cur_cluster;
  uint32_t off_in_cluster; /* bajty */
  lfn_accum_t lacc;
} dir_iter_t;

int fat32_readdir_first(fat32_volume_t *vol, uint32_t dir_cluster,
                        fat32_file_t **out) {
  dir_iter_t *it = (dir_iter_t *)fat32_malloc(sizeof(dir_iter_t));
  if (!it) return -1;
  ZERO(it, sizeof(*it));
  it->base.vol = vol;
  it->base.is_dir = true;
  it->base.cluster_buf =
      (uint8_t *)fat32_malloc(vol->sectors_per_cluster * vol->bytes_per_sector);
  if (!it->base.cluster_buf) {
    fat32_free(it);
    return -2;
  }
  it->cur_cluster = dir_cluster;
  lfn_reset(&it->lacc);

  /* ładujemy pierwszy klaster */
  if (read_entire_cluster(vol, it->cur_cluster, it->base.cluster_buf)) {
    fat32_free(it->base.cluster_buf);
    fat32_free(it);
    return -3;
  }
  *out = (fat32_file_t *)it;
  return 0;
}

int fat32_readdir_next(fat32_file_t *dir_handle, fat32_dirent_info_t *out) {
  dir_iter_t *it = (dir_iter_t *)dir_handle;
  const uint32_t dsz = sizeof(fat32_dirent_t);
  const uint32_t csz =
      it->base.vol->sectors_per_cluster * it->base.vol->bytes_per_sector;

  while (1) {
    if (it->off_in_cluster >= csz) {
      /* następny klaster w łańcuchu */
      uint32_t nxt;
      if (fat32_next_cluster(it->base.vol, it->cur_cluster, &nxt)) return -1;
      if (fat32_is_eoc(nxt)) return 1; /* koniec */
      it->cur_cluster = nxt;
      it->off_in_cluster = 0;
      if (read_entire_cluster(it->base.vol, it->cur_cluster,
                              it->base.cluster_buf)) return -2;
      lfn_reset(&it->lacc);
    }

    fat32_dirent_t *de =
        (fat32_dirent_t *)(it->base.cluster_buf + it->off_in_cluster);
    it->off_in_cluster += dsz;

    if (de->name[0] == 0x00) return 1; /* koniec katalogu */
    if (de->name[0] == 0xE5) { lfn_reset(&it->lacc); continue; } /* usunięty */

    if (de->attr == FAT32_ATTR_LFN) { lfn_feed(&it->lacc, (fat32_lfn_t *)de); continue; }
    if (de->attr & FAT32_ATTR_VOLUME_ID) { lfn_reset(&it->lacc); continue; }

    build_dirent_info(de, &it->lacc, out);
    lfn_reset(&it->lacc);
    return 0;
  }
}

void fat32_readdir_close(fat32_file_t *dir_handle) {
  dir_iter_t *it = (dir_iter_t *)dir_handle;
  if (it->base.cluster_buf) fat32_free(it->base.cluster_buf);
  fat32_free(it);
}