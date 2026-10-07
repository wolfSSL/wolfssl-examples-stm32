/*
 * STM32H753ZI linker script - standalone (no bootloader).
 *   FLASH: 2048 KB @ 0x08000000
 *   RAM:    512 KB @ 0x24000000 (AXI SRAM / RAM_D1)
 * Thin form: shared SECTIONS in boards/common/sections.ld.
 */
ENTRY(Reset_Handler)

_estack = 0x24080000;    /* 0x24000000 + 512K */
_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x10000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
}

INCLUDE "sections.ld"
