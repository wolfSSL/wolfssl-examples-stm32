/*
 * STM32WB55RG linker script - standalone (no bootloader).
 *
 * Flash: 1024 KB @ 0x08000000
 * RAM:   192 KB  @ 0x20000000
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x8000;
_Min_Stack_Size = DEFINED(_stack_size) ? _stack_size : 0x8000;
_heap_limit = _estack - _Min_Stack_Size;

/* WB55 startup_stm32wb55xx.s references MB_MEM2 (M0+/BLE shared
 * memory). SRAM2A/B (64K at 0x20030000) is reserved for the radio
 * core and IPCC mailboxes -- we don't use it from M4. Provide an
 * empty range so the startup zero-init copies nothing. */
_sMB_MEM2  = 0;
_eMB_MEM2  = 0;
_siMB_MEM2 = 0;

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 192K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

INCLUDE "sections.ld"
