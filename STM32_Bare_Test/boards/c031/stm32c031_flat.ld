/*
 * STM32C031C6 linker script - standalone (no bootloader).
 *
 * Flash: 32 KB @ 0x08000000  -- smallest in the matrix
 * RAM:   12 KB @ 0x20000000
 *
 * With the STM32_C031_TRIM cut down to AES-CBC + SHA-256 + ChaCha/Poly
 * the binary fits in ~28 KB. wolfcrypt_test is disabled.
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x200;   /* 512 B heap  -- tight RAM */
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x800;   /* 2 KB stack  */
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 12K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
}

INCLUDE "sections.ld"
