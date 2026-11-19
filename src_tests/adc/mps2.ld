/* Both MPS2 boards load the ELF into SRAM, including initialized data. */
ENTRY(run_tests)
MEMORY {
    CODE (rx) : ORIGIN = 0, LENGTH = 256K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}
_stack_top = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
    .vectors : { KEEP(*(.test_vectors)) } > CODE
    .text : { *(.text*) *(.rodata*) } > CODE
    .data : { *(.data*) } > RAM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
