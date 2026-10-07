/*
 * STM32F303xE linker script - standalone (no bootloader).
 *
 * Flash: 512 KB @ 0x08000000
 * RAM:   64 KB  @ 0x20000000
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x4000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x2000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}

INCLUDE "sections.ld"
