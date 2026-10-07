/*
 * STM32U385RG linker script - standalone (no bootloader).
 *   FLASH: 1024 KB @ 0x08000000
 *   RAM:    192 KB @ 0x20000000 (+ RAM2 64 KB @ 0x20030000, unused)
 * Thin form: shared SECTIONS in boards/common/sections.ld.
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x10000;
_heap_limit = _estack - _Min_Stack_Size;
_sstack = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 192K
  RAM2  (xrw) : ORIGIN = 0x20030000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

INCLUDE "sections.ld"
