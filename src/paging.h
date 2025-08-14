/*
 * [Cygnus] - [src/paging.h]
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
#ifndef CYGNUS_PAGING_H
#define CYGNUS_PAGING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ====== Arch constants ====== */
#define PAGE_SIZE 4096u
#define PAGE_SHIFT 12u
#define PAGE_MASK (~(PAGE_SIZE - 1u))

/* CR0 flags */
#define CR0_PG (1u << 31) /* Paging enable */
#define CR0_WP (1u << 16) /* Write protect in supervisor */

/* PDE/PTE flags (i386) */
enum {
  PG_PRESENT = 1u << 0,
  PG_RW = 1u << 1,
  PG_USER = 1u << 2,
  PG_PWT = 1u << 3, /* write-through */
  PG_PCD = 1u << 4, /* cache disable */
  PG_ACCESSED = 1u << 5,
  PG_DIRTY = 1u << 6, /* PTE only */
  PG_PS = 1u << 7,    /* PDE only (4MiB pages; we don't use) */
  PG_GLOBAL = 1u << 8
};

typedef uint32_t pte_t;
typedef uint32_t pde_t;

/* ====== Public API ====== */

/**
 * Initialize paging and a simple physical frame allocator.
 *
 * @param kernel_phys_start  physical start of the kernel (page-aligned
 * recommended)
 * @param kernel_phys_end    physical end   of the kernel (exclusive; will be
 * rounded up)
 * @param phys_mem_top       total usable physical memory top (exclusive), e.g.,
 * from BIOS/e820
 *
 * This performs identity mapping for [0, kernel_phys_end) and enables CR4 PGE
 * if you later add it; currently we set CR0.WP and leave CR4 as-is.
 */
void paging_setup(uintptr_t kernel_phys_start, uintptr_t kernel_phys_end,
                  uintptr_t phys_mem_top);

/** Enable paging (loads CR3 and sets CR0.PG|CR0.WP). Call after paging_setup.
 */
void paging_enable(void);

/** Map a single 4KiB page: phys -> virt with flags (PG_PRESENT|PG_RW|PG_USER
 * etc.). */
void paging_map_page(uintptr_t phys, uintptr_t virt, uint32_t flags);

/** Unmap a single 4KiB page at virtual address 'virt'. Frees the physical frame
 * if own_frame==true. */
void paging_unmap_page(uintptr_t virt, bool own_frame);

/** Flush a single TLB entry. */
void paging_invalidate(uintptr_t virt);

/** Translate virtual address to physical. Returns 0 on not-present. */
uintptr_t paging_virt_to_phys(uintptr_t virt);

/** Map a contiguous range (size rounded up). Convenience around
 * paging_map_page. */
void paging_map_range(uintptr_t phys_start, uintptr_t virt_start, size_t size,
                      uint32_t flags);

/** Unmap a contiguous range (size rounded up). */
void paging_unmap_range(uintptr_t virt_start, size_t size, bool own_frames);

/** Page fault ISR entry. Pass CR2 (fault VA) and error code from the CPU. */
void page_fault_isr(uintptr_t cr2, uint32_t err);

/* ====== Simple Physical Frame Allocator (4KiB frames) ====== */

/** Allocate one free physical frame (4KiB). Returns 0 on OOM. */
uintptr_t pmm_alloc_frame(void);

/** Free a previously allocated physical frame (address must be 4KiB aligned).
 */
void pmm_free_frame(uintptr_t phys);

/** Mark a physical region [start, end) as used (e.g., MMIO, ACPI, etc.). */
void pmm_mark_region_used(uintptr_t start, uintptr_t end);

/** Mark a physical region [start, end) as free (careful!). */
void pmm_mark_region_free(uintptr_t start, uintptr_t end);

#endif /* CYGNUS_PAGING_H */