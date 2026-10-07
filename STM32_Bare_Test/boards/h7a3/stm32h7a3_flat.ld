/*
 * STM32H7A3ZI-Q linker script - standalone (no bootloader).
 *
 * Flash: 2048 KB @ 0x08000000 (dual-bank, 1MB+1MB internal)
 * RAM:    256 KB @ 0x24000000 (AXISRAM1; the chip also has 384K
 *         AXISRAM2 at 0x24040000 and 384K AXISRAM3 at 0x240A0000,
 *         but the BARE test fits comfortably in AXISRAM1).
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x10000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x10000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x24000000, LENGTH = 256K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
}

INCLUDE "sections.ld"
