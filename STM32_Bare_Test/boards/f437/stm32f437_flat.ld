/*
 * STM32F437II linker script - standalone (no bootloader).
 *
 * Flash: 2048 KB @ 0x08000000
 * RAM:   192 KB  @ 0x20000000
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x14000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 192K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  CCMRAM (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
}

INCLUDE "sections.ld"
