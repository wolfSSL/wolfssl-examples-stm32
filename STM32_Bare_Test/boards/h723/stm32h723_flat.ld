/*
 * STM32H723ZG linker script - standalone (no bootloader).
 *
 * Flash: 1024 KB @ 0x08000000
 * RAM:   320 KB  @ 0x24000000
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x10000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x24000000, LENGTH = 320K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

INCLUDE "sections.ld"
