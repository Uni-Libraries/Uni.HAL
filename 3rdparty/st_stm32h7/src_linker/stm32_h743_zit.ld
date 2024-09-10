/* Entry Point */
ENTRY(Reset_Handler)

/* Specify the memory areas */
MEMORY
{
    DTCMRAM      (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K
    RAM_D1       (xrw)  : ORIGIN = 0x24000000, LENGTH = 512K

    RAM_D2_ETH_B (xrw)  : ORIGIN = 0x30000000, LENGTH = 64K
    RAM_D2_ETH_R (xrw)  : ORIGIN = 0x30010000, LENGTH = 1K
    RAM_D2_ETH_T (xrw)  : ORIGIN = 0x30010400, LENGTH = 1K
    RAM_D2_ADC   (xrw)  : ORIGIN = 0x30010800, LENGTH = 1K
    RAM_D2       (xrw)  : ORIGIN = 0x30010C00, LENGTH = 256K - 64K - 1K - 1K - 1K

    RAM_D2_SRAM3 (rw)   : ORIGIN = 0x30040000, LENGTH = 32K
    RAM_D3       (rw)   : ORIGIN = 0x38000000, LENGTH = 64K
    ITCMRAM      (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K
    FLASH        (rx)   : ORIGIN = 0x8000000 , LENGTH = 2048K
}

/* Regions memory access flags */
PHDRS
{
  isr_vector PT_LOAD FLAGS(5); /* RX */
  text       PT_LOAD FLAGS(5); /* RX */
  arm        PT_LOAD FLAGS(5); /* RX */
  arm_extab  PT_LOAD FLAGS(5); /* RX */
  rodata     PT_LOAD FLAGS(6); /* RW */
  data       PT_LOAD FLAGS(6); /* RW */
  bss        PT_LOAD FLAGS(6); /* RW */
  heap       PT_LOAD FLAGS(6); /* RW */
}

/* Stack */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);

/* Heap */
_Min_Heap_Size  = 0x200; /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */


/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH :isr_vector

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH :text

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH :rodata

  .ARM.extab   : {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH :arm_extab

  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH :arm

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >DTCMRAM AT> FLASH :data

  
  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >DTCMRAM :bss

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >DTCMRAM :heap

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : {
    *(.ARM.attributes)
  }

  .FREERTOS_HEAP (NOLOAD) : {
    . = ALIGN(32);
    *(.FREERTOS_HEAP)
  } >RAM_D1

  .RAM_D1 (NOLOAD) : {
    . = ALIGN(32);
      *(.RAM_D1)
  } >RAM_D1

  .EthBuffersSection (NOLOAD) : {
      . = ALIGN(32);
      *(.EthBuffersSection)
  } >RAM_D2_ETH_B

  .RxDecripSection (NOLOAD) : {
      . = ALIGN(32);
      *(.RxDescripSection)
  } >RAM_D2_ETH_R

  .TxDescripSection (NOLOAD) : {
      . = ALIGN(32);
      *(.TxDescripSection)
  } >RAM_D2_ETH_T

  .AdcSection (NOLOAD) : {
      . = ALIGN(32);
      *(.AdcSection)
  } >RAM_D2_ADC
}
