/**
******************************************************************************
* @file    stm32c562_flat.ld
* @brief   Linker File for STM32C562xx -- NUCLEO-C562RE
*
* Flash : 512 KB @ 0x08000000
* SRAM  : 128 KB @ 0x20000000 (SRAM1 64 KB + SRAM2 64 KB contiguous)
******************************************************************************
*/

ENTRY(Reset_Handler)

HEAP_SIZE  = 0x2000;   /* 8 KB -- enough for the wolfCrypt test/bench paths */
STACK_SIZE = DEFINED(_stack_size) ? _stack_size : 0x8000;   /* 32 KB -- ECC + Dilithium small-stack headroom */

_Min_Heap_Size  = HEAP_SIZE;
_Min_Stack_Size = STACK_SIZE;


MEMORY
{
  ROM (rx)  : org = 0x08000000, len = 0x80000
  RAM (xrw) : org = 0x20000000, len = 0x20000
}

SECTIONS
{
  .vectors :
  {
    . = ALIGN(8);
    KEEP(*(.vectors));
    . = ALIGN(8);
  } > ROM

  .text :
  {
    . = ALIGN(8);
    *(.text);
    *(.text*);
    *(.glue_7);
    *(.glue_7t);
    *(.eh_frame);
    KEEP (*(.init));
    KEEP (*(.fini));
    . = ALIGN(8);
    _etext = .;
  } > ROM

  .rodata :
  {
    . = ALIGN(8);
    *(.rodata);
    *(.rodata*);
    . = ALIGN(8);
  } > ROM

  .ARM.extab (READONLY) :
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } > ROM

  .ARM (READONLY) :
  {
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx*);
    __exidx_end = .;
    . = ALIGN(8);
  } > ROM

  .preinit_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*));
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } > ROM

  .init_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)));
    KEEP (*(.init_array*));
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } > ROM

  .fini_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)));
    KEEP (*(.fini_array*));
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } > ROM

  .copy.table (READONLY) :
  {
    . = ALIGN(8);
    __copy_table_start__ = .;
    LONG(LOADADDR(.data));
    LONG (ADDR(.data));
    LONG (SIZEOF(.data) / 4);
    __copy_table_end__ = .;
  } > ROM

  .zero.table (READONLY) :
  {
    . = ALIGN(8);
    __zero_table_start__ = .;
    LONG (ADDR(.bss));
    LONG (SIZEOF(.bss) / 4);
    __zero_table_end__ = .;
  } > ROM

  .data :
  {
    . = ALIGN(8);
    _sidata = LOADADDR(.data);
    __data_start__ = .;
    _sdata = .;
    *(.data);
    *(.data*);
    . = ALIGN(8);
    _edata = .;
  } > RAM AT> ROM

  .bss :
  {
    . = ALIGN(8);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss);
    *(.bss*);
    *(COMMON);
    . = ALIGN(8);
    _ebss = .;
    __bss_end__ = _ebss;
  } > RAM

  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE (end = .);
    PROVIDE (_end = .);
    _heap_start = .;
    . += HEAP_SIZE;
    . = ALIGN(8);
    _heap_end = .;
    PROVIDE (_heap_limit = .);
    __HeapLimit = .;
  } > RAM

  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    __StackLimit = .;
    . += STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
    _estack = .;
    __stack = .;
  } > RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 :
  {
    *(.ARM.attributes)
  }
}
