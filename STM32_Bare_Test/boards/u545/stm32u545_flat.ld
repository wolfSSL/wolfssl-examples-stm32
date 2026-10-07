/*
 * STM32U545 linker script - standalone (no bootloader).
 *   ROM: 512K @ 0x08000000   RAM: 256K @ 0x20000000   SRAM4 16K @ 0x28000000
 * Thin form: shared ALIGN(8)/ROM SECTIONS in boards/common/sections-align8.ld.
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x10000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 256K
  ROM   (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  SRAM4 (xrw) : ORIGIN = 0x28000000, LENGTH = 16K
}

INCLUDE "sections-align8.ld"
