/*
 * [Cygnus] - [src/paging.c]
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
#include "paging.h"

/* We avoid libc; provide tiny memset/memset32/memcpy. */
static void *k_memset(void *dst, int v, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  for (size_t i = 0; i < n; ++i)
    d[i] = (uint8_t)v;
  return dst;
}
static void k_memset32(uint32_t *dst, uint32_t v, size_t n_words) {
  for (size_t i = 0; i < n_words; ++i)
    dst[i] = v;
}
static void *k_memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; ++i)
    d[i] = s[i];
  return dst;
}

/* Align helpers */
static inline uintptr_t align_down(uintptr_t x, uintptr_t a) {
  return x & ~(a - 1u);
}
static inline uintptr_t align_up(uintptr_t x, uintptr_t a) {
  return (x + a - 1u) & ~(a - 1u);
}

/* ====== Physical frame allocator (bitmap) ======
   Supports up to MAX_PHYS (default 4 GiB). One bit per 4KiB frame.
*/
#ifndef MAX_PHYS_BYTES
#define MAX_PHYS_BYTES (0x100000000ull) /* 4 GiB */
#endif

#define MAX_FRAMES ((size_t)(MAX_PHYS_BYTES / PAGE_SIZE))
#define BITMAP_WORDS ((MAX_FRAMES + 31u) / 32u)

/* 4 GiB / 4K = 1,048,576 frames -> 1,048,576 bits -> 32,768 u32s -> 128 KiB */
static uint32_t frame_bitmap[BITMAP_WORDS];
static size_t total_frames = 0;
static size_t first_usable_frame = 0; /* usually after kernel end */

static inline void fb_set(size_t i) {
  frame_bitmap[i >> 5] |= (1u << (i & 31u));
}
static inline void fb_clear(size_t i) {
  frame_bitmap[i >> 5] &= ~(1u << (i & 31u));
}
static inline bool fb_test(size_t i) {
  return (frame_bitmap[i >> 5] >> (i & 31u)) & 1u;
}

static inline size_t phys_to_frame(uintptr_t phys) {
  return (size_t)(phys >> PAGE_SHIFT);
}
static inline uintptr_t frame_to_phys(size_t f) {
  return (uintptr_t)f << PAGE_SHIFT;
}

static size_t fb_find_first_zero_from(size_t start_f) {
  if (start_f >= total_frames)
    return (size_t)-1;
  size_t w = start_f >> 5;
  uint32_t word = frame_bitmap[w];
  /* mask off bits before start_f within the starting word */
  uint32_t pre_mask = (1u << (start_f & 31u)) - 1u;
  word |= pre_mask;

  /* scan first partial word */
  if (~word) {
    for (int b = (int)(start_f & 31u); b < 32; ++b) {
      if (!(frame_bitmap[w] & (1u << b)))
        return (w << 5) | (size_t)b;
    }
  }
  /* scan full words */
  for (size_t i = w + 1; i < (total_frames + 31u) / 32u; ++i) {
    if (~frame_bitmap[i]) {
      for (int b = 0; b < 32; ++b) {
        size_t idx = (i << 5) | (size_t)b;
        if (idx >= total_frames)
          break;
        if (!(frame_bitmap[i] & (1u << b)))
          return idx;
      }
    }
  }
  return (size_t)-1;
}

/* Public PMM API */
uintptr_t pmm_alloc_frame(void) {
  size_t idx = fb_find_first_zero_from(first_usable_frame);
  if (idx == (size_t)-1)
    return 0;
  fb_set(idx);
  return frame_to_phys(idx);
}
void pmm_free_frame(uintptr_t phys) {
  if (!phys)
    return;
  size_t f = phys_to_frame(phys);
  if (f < total_frames)
    fb_clear(f);
}
void pmm_mark_region_used(uintptr_t start, uintptr_t end) {
  start = align_down(start, PAGE_SIZE);
  end = align_up(end, PAGE_SIZE);
  for (uintptr_t p = start; p < end; p += PAGE_SIZE) {
    size_t f = phys_to_frame(p);
    if (f < total_frames)
      fb_set(f);
  }
}
void pmm_mark_region_free(uintptr_t start, uintptr_t end) {
  start = align_down(start, PAGE_SIZE);
  end = align_up(end, PAGE_SIZE);
  for (uintptr_t p = start; p < end; p += PAGE_SIZE) {
    size_t f = phys_to_frame(p);
    if (f < total_frames)
      fb_clear(f);
  }
}

/* ====== Paging structures ====== */
#define ENTRIES_PER_TABLE 1024
#define ENTRIES_PER_DIR 1024

/* Page directory (physical pointer goes to CR3). We store it statically. */
static
    __attribute__((aligned(4096))) pde_t kernel_page_directory[ENTRIES_PER_DIR];
/* We allocate page tables on demand (each is 4KiB, aligned), grabbing frames
 * via PMM. */

/* Helpers to access PDE/PTE */
static inline pde_t *current_pdir(void) { return kernel_page_directory; }

static pte_t *get_pte(uintptr_t virt, bool create_table) {
  pde_t *pdir = current_pdir();
  uint32_t pd_index = (virt >> 22) & 0x3FFu;
  uint32_t pt_index = (virt >> 12) & 0x3FFu;

  pde_t pde = pdir[pd_index];
  if (!(pde & PG_PRESENT)) {
    if (!create_table)
      return NULL;

    /* allocate a physical frame for the PT */
    uintptr_t pt_phys = pmm_alloc_frame();
    if (!pt_phys)
      return NULL;

    /* map the PT temporarily to zero it: we'll identity-map PTs to simplify */
    /* Since paging may not be enabled yet during setup, memset by physical
       address is fine here because kernel runs identity-mapped during setup.
       After enable, all PT frames we allocate are also identity-mapped by
       design of paging_setup's identity range covering kernel end. */
    k_memset((void *)pt_phys, 0, PAGE_SIZE);

    /* present, RW, supervisor */
    pdir[pd_index] = (pde_t)(pt_phys | PG_PRESENT | PG_RW);
  }

  uintptr_t pt_phys = (uintptr_t)(pde & PAGE_MASK);
  pte_t *pt = (pte_t *)pt_phys; /* identity assumption for paging_setup */
  return &pt[pt_index];
}

void paging_map_page(uintptr_t phys, uintptr_t virt, uint32_t flags) {
  phys = align_down(phys, PAGE_SIZE);
  virt = align_down(virt, PAGE_SIZE);

  pte_t *pte = get_pte(virt, /*create_table=*/true);
  if (!pte)
    return; /* optionally assert/panic */

  *pte = (pte_t)(phys | (flags | PG_PRESENT));

  /* tlb shootdown for this VA */
  paging_invalidate(virt);
}

void paging_unmap_page(uintptr_t virt, bool own_frame) {
  virt = align_down(virt, PAGE_SIZE);
  pte_t *pte = get_pte(virt, /*create_table=*/false);
  if (!pte)
    return;
  if (*pte & PG_PRESENT) {
    uintptr_t phys = (uintptr_t)(*pte & PAGE_MASK);
    *pte = 0;
    paging_invalidate(virt);
    if (own_frame)
      pmm_free_frame(phys);
  }
}

void paging_map_range(uintptr_t phys_start, uintptr_t virt_start, size_t size,
                      uint32_t flags) {
  uintptr_t p = align_down(phys_start, PAGE_SIZE);
  uintptr_t v = align_down(virt_start, PAGE_SIZE);
  uintptr_t end = align_up(virt_start + size, PAGE_SIZE);
  while (v < end) {
    paging_map_page(p, v, flags);
    p += PAGE_SIZE;
    v += PAGE_SIZE;
  }
}

void paging_unmap_range(uintptr_t virt_start, size_t size, bool own_frames) {
  uintptr_t v = align_down(virt_start, PAGE_SIZE);
  uintptr_t end = align_up(virt_start + size, PAGE_SIZE);
  while (v < end) {
    paging_unmap_page(v, own_frames);
    v += PAGE_SIZE;
  }
}

uintptr_t paging_virt_to_phys(uintptr_t virt) {
  pte_t *pte = get_pte(virt, /*create_table=*/false);
  if (!pte)
    return 0;
  if (!(*pte & PG_PRESENT))
    return 0;
  uintptr_t phys_page = (uintptr_t)(*pte & PAGE_MASK);
  return phys_page | (virt & (PAGE_SIZE - 1u));
}

/* ====== CRx helpers ====== */
static inline void write_cr3(uintptr_t phys) {
  __asm__ volatile("mov %0, %%cr3" ::"r"(phys) : "memory");
}
static inline uintptr_t read_cr0(void) {
  uintptr_t v;
  __asm__ volatile("mov %%cr0, %0" : "=r"(v));
  return v;
}
static inline void write_cr0(uintptr_t v) {
  __asm__ volatile("mov %0, %%cr0" ::"r"(v) : "memory");
}

/* Invalidate one page */
void paging_invalidate(uintptr_t virt) {
  __asm__ volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

/* ====== Setup & enable ====== */
void paging_setup(uintptr_t kernel_phys_start, uintptr_t kernel_phys_end,
                  uintptr_t phys_mem_top) {
  /* Initialize PMM bitmap. Everything starts 'used', then we free [0,
   * phys_mem_top) and re-mark kernel used. */
  total_frames = (size_t)(phys_mem_top >> PAGE_SHIFT);
  if (total_frames > MAX_FRAMES)
    total_frames = MAX_FRAMES;

  k_memset32(frame_bitmap, 0xFFFFFFFFu, BITMAP_WORDS); /* all used */
  /* free usable RAM [0, phys_mem_top) */
  pmm_mark_region_free(0, phys_mem_top);

  /* mark kernel image frames used */
  kernel_phys_start = align_down(kernel_phys_start, PAGE_SIZE);
  kernel_phys_end = align_up(kernel_phys_end, PAGE_SIZE);
  pmm_mark_region_used(kernel_phys_start, kernel_phys_end);

  /* Reserve page directory's physical page (it’s static & in .bss/.data) */
  uintptr_t pdir_phys = (uintptr_t)kernel_page_directory;
  pmm_mark_region_used(align_down(pdir_phys, PAGE_SIZE),
                       align_up(pdir_phys + PAGE_SIZE, PAGE_SIZE));

  /* Identity map [0, kernel_phys_end) so early kernel and PT frames are
   * reachable by their physical addrs. */
  k_memset(kernel_page_directory, 0, sizeof(kernel_page_directory));
  paging_map_range(0, 0, (size_t)kernel_phys_end, PG_RW); /* supervisor RW */

  /* Pick the first usable frame after kernel as a starting hint for allocation
   */
  first_usable_frame = phys_to_frame(kernel_phys_end);

  /* Load CR3 with the page directory physical address (identity assumed) */
  write_cr3((uintptr_t)kernel_page_directory);
}

void paging_enable(void) {
  /* Enable paging + supervisor write-protect. */
  uintptr_t cr0 = read_cr0();
  cr0 |= (CR0_PG | CR0_WP);
  write_cr0(cr0);

  /* From here, paging is on. We kept identity mappings for kernel so execution
   * continues seamlessly. */
}

/* ====== Page fault handler ====== */
/* Error code bits: P=0 not-present/1 protection, W=1 write, U=1 user, Rsvd=1
 * reserved-bit, I=1 instr fetch */
static const char *pf_reason(uint32_t err) {
  static char buf[64];
  /* tiny formatter */
  const char *np = (err & 1) ? "protection" : "not-present";
  const char *wr = (err & 2) ? "write" : "read";
  const char *us = (err & 4) ? "user" : "kernel";
  const char *rs = (err & 8) ? "rsvd" : "ok";
  const char *ifx = (err & 16) ? "exec" : "data";
  /* very small sprintf */
  char *p = buf;
  const char *parts[] = {np, wr, us, rs, ifx};
  for (int i = 0; i < 5; ++i) {
    const char *s = parts[i];
    while (*s)
      *p++ = *s++;
    if (i != 4)
      *p++ = ' ';
  }
  *p = 0;
  return buf;
}

/* You must wire this into your IDT: ISR 14 should call
 * page_fault_isr(read_cr2(), err). */
void page_fault_isr(uintptr_t cr2, uint32_t err) {
  (void)cr2;
  (void)err;
  /* For now, just hang. Replace with your kernel's panic/log. */
  /* Example (if you have a logger): kprintf("PAGE FAULT @%p: %s (err=%#x)\n",
   * cr2, pf_reason(err), err); */
  for (;;) {
    __asm__ volatile("hlt");
  }
}