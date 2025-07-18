#
# [Cygnus] - [boot.s]
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
    .section .multiboot_header
    .align 4
    /* Multiboot1 header: magic, flags=0, checksum */
    .long 0x1BADB002
    .long 0x0
    .long -(0x1BADB002 + 0x0)

    .text
    .global start
start:
    cli
    movl $stack_top, %esp
    call kmain

.hang:
    hlt
    jmp .hang

    .section .bss
    .align 4
stack_bottom:
    .skip 8192
stack_top:
