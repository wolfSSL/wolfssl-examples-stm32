/*
 * STM32G071RB linker script - standalone (no bootloader).
 *
 * Flash: 128 KB @ 0x08000000
 * RAM:    36 KB @ 0x20000000
 *
 * Tight memory like U083 (256K/40K) but with 128K flash. Use the
 * STM32_G071_TRIM build flag in user_settings.h to drop heavy-tail
 * algorithms (P-384/P-521, SHA-384/512, Curve25519, Ed25519). The
 * trimmed binary should fit in <120 KB text.
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x800;   /* 2K heap  -- tight RAM */
/* 4K stack -- WOLFSSL_SMALL_STACK pattern. src/stubs.c also refuses to grow
 * the heap into the live stack, so an optimistic carve here degrades to a
 * clean allocation failure rather than heap/stack corruption. */
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x1000;
_heap_limit = _estack - _Min_Stack_Size;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 36K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
}

INCLUDE "sections.ld"
