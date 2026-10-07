/*
 * STM32WBA52CG linker script - standalone (no bootloader).
 *
 * Flash: 1024 KB @ 0x08000000
 * RAM:   128 KB  @ 0x20000000
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_sstack = _estack - _Min_Stack_Size;

_Min_Heap_Size  = 0x4000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0xC000;   /* 48 KB - Dilithium PQC=1 keygen needs >16 KB even with WOLFSSL_DILITHIUM_SMALL */
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

INCLUDE "sections.ld"
