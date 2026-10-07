/*
 * STM32H563ZI linker script - standalone (no bootloader).
 *   FLASH: 2048 KB @ 0x08000000
 *   RAM:    640 KB @ 0x20000000
 * Thin form: board-specific MEMORY/symbols here, shared SECTIONS in
 * boards/common/sections.ld.
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x8000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 640K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
}

INCLUDE "sections.ld"
