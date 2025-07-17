ENTRY(start)

SECTIONS {
  /* place code at 1 MiB */
  . = 0x00100000;

  /* multiboot header & entry point */
  .multiboot : {
    KEEP(*(.multiboot_header))
    *(.text*)
  } 

  .rodata : { *(.rodata*) }
  .data   : { *(.data*) }
  .bss    : { *(.bss*) *(COMMON) }
}