/* Both MPS2 boards load the ELF into SRAM, including initialized data. */
ENTRY(run_tests)
MEMORY {
    CODE (rx) : ORIGIN = 0, LENGTH = 256K
    /* The dynamic-only FreeRTOS port places its 100 KiB heap in SRAM. */
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
_stack_top = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
    .vectors : { KEEP(*(.test_vectors)) } > CODE
    .text : { *(.text*) *(.rodata*) } > CODE
    .data : { *(.data*) } > RAM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
