/* STM32V873XJ SRAM-execution linker script - STM32_Bare_Test.
 *
 * Everything (vectors, code, data) lives in AXI SRAM; nothing touches
 * flash at runtime. Purpose: the freeze-discriminator experiment -- if the
 * placement-sensitive wolfcrypt_test freeze never occurs when executing
 * from SRAM, the flash fetch path is implicated. Load with CubeProgrammer
 * (-w app.elf) and start at the Reset_Handler entry; the startup sets its
 * own MSP/MSPLIM so no vector-table SP fetch is needed.
 *
 * SRAM per ST's map: 0x34030000 + 1216K. Code 512K + data 704K.
 */
ENTRY(Reset_Handler)

HEAP_SIZE  = 0x60000;   /* 384K heap  - ML-DSA/ML-KEM key material */
STACK_SIZE = DEFINED(_stack_size) ? _stack_size : 0x40000;   /* 256K stack - SP-math ECC and ML-DSA sign frames */

/* Aliases for the STM32_Bare_Test STACK=1 adapter -- see board_common.c. */
_Min_Heap_Size  = HEAP_SIZE;
_Min_Stack_Size = STACK_SIZE;

MEMORY
{
  ROM (rx)  : org = 0x34030000, len = 0x80000
  /* ST's official linker script (CubeIDE project) places RAM at 0x34030000
   * with 1216K total; the low 192K below it is NOT ordinary AXI SRAM. With
   * data/heap in the low region, wolfcrypt_test stalled at layout-sensitive
   * points (moved when printfs were added); adopting ST's map is the fix
   * under test. */
  RAM (xrw) : org = 0x340B0000, len = 0xB0000
}

SECTIONS
{
  .vectors :
  {
    . = ALIGN(8);
    KEEP(*(.vectors));
    . = ALIGN(8);
  } > ROM

  .text :
  {
    . = ALIGN(8);
    *(.text);
    *(.text*);
    *(.glue_7);
    *(.glue_7t);
    *(.eh_frame);
    KEEP (*(.init));
    KEEP (*(.fini));
    . = ALIGN(8);
    _etext = .;
  } > ROM

  .rodata :
  {
    . = ALIGN(8);
    *(.rodata);
    *(.rodata*);
    . = ALIGN(8);
  } > ROM

  .ARM.extab (READONLY) :
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } > ROM

  .ARM (READONLY) :
  {
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx*);
    __exidx_end = .;
    . = ALIGN(8);
  } > ROM

  .preinit_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*));
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } > ROM

  .init_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)));
    KEEP (*(.init_array*));
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } > ROM

  .fini_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)));
    KEEP (*(.fini_array*));
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } > ROM

  .copy.table (READONLY) :
  {
    . = ALIGN(8);
    __copy_table_start__ = .;
    LONG (LOADADDR(.data));
    LONG (ADDR(.data));
    LONG (SIZEOF(.data) / 4);
    __copy_table_end__ = .;
  } > ROM

  .zero.table (READONLY) :
  {
    . = ALIGN(8);
    __zero_table_start__ = .;
    LONG (ADDR(.bss));
    LONG (SIZEOF(.bss) / 4);
    __zero_table_end__ = .;
  } > ROM

  .data :
  {
    . = ALIGN(8);
    _sidata = LOADADDR(.data);
    __data_start__ = .;
    _sdata = .;
    *(.data);
    *(.data*);
    . = ALIGN(8);
    _edata = .;
  } > RAM AT> ROM

  .bss :
  {
    . = ALIGN(8);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss);
    *(.bss*);
    *(COMMON);
    . = ALIGN(8);
    _ebss = .;
    __bss_end__ = _ebss;
  } > RAM

  /* Survives reset: NOT listed in .zero.table, so __PROGRAM_START leaves it
   * alone. The SWD console log lives here, which lets a completed (or hung)
   * run be read back after a mode=UR connect -- reset does not clear SRAM. */
  .noinit (NOLOAD) :
  {
    . = ALIGN(8);
    *(.noinit)
    *(.noinit*)
    . = ALIGN(8);
  } > RAM

  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE (end = .);
    PROVIDE (_end = .);
    _heap_start = .;
    . += HEAP_SIZE;
    . = ALIGN(8);
    _heap_end = .;
    PROVIDE (_heap_limit = .);
    __HeapLimit = .;
  } > RAM

  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    __StackLimit = .;
    . += STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
    _estack = .;
    __stack = .;
  } > RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 :
  {
    *(.ARM.attributes)
  }
}
