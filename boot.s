#
# [Cygnus] - [src/kernel.c]
#
# Copyright (C) [2025] [Szymon Grajner]
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the European Union Public Licence (EUPL) V.1.2 or - as
# soon as they will be approved by the European Commission - subsequent
# versions of the EUPL (the "Licence").
#
# You may not use this work except in compliance with the Licence.
# You may obtain a copy of the Licence at:
# https://joinup.ec.europa.eu/software/page/eupl/licence-eupl
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Licence is distributed on an "AS IS" basis,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the Licence for the specific language governing permissions and
# limitations under the Licence.
#
    .code32

    /* ===== Multiboot v1 header (ALIGN | MEMINFO) ===== */
    .section .multiboot, "a"
    .align 4
    .set MB_MAGIC,    0x1BADB002
    .set MB_FLAGS,    (1 << 0) | (1 << 1)
    .set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM

    /* ===== Wejście jądra ===== */
    .section .text
    .globl start
    .type  start, @function
    .extern kmain

start:
    cli                 /* wyłączamy przerwania */
    cld                 /* DF=0 dla operacji string */
    movl $stack_top, %esp
    xorl %ebp, %ebp

    /* Jeśli kiedyś chcemy przekazać magic/mbi:
       ; pushl %ebx     ; ptr do multiboot info
       ; pushl %eax     ; magic 0x2BADB002
       ; call kmain
       ; add $8, %esp
    */
    call kmain

.hang:
    hlt
    jmp .hang

/* Zamykamy symbol w tej samej sekcji, żeby .size miało stałą wartość */
.Lstart_end:
    .size start, .Lstart_end - start

    /* ===== Stos (BSS) ===== */
    .section .bss
    .align 16
stack_bottom:
    .skip 16384         /* 16 KiB */
stack_top:
