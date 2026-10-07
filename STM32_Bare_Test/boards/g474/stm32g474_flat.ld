/*
 * STM32G474RE linker script - standalone (no bootloader).
 *
 * Flash: 512 KB @ 0x08000000
 * RAM:   96 KB  @ 0x20000000 (SRAM1 80 KB + SRAM2 16 KB contiguous).
 *                             CCM-SRAM 32 KB at 0x10000000 not used here.
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x2000;   /* 8 KB */
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x8000;   /* 32 KB - PQC=1 keygen headroom */
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 96K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}

INCLUDE "sections.ld"
