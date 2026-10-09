/* board.h
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * This file is part of wolfssl-examples.
 *
 * wolfssl-examples is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfssl-examples is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* wolfDemo board support shared by the test and benchmark entry points */

#ifndef WOLFDEMO_BOARD_H
#define WOLFDEMO_BOARD_H

#include <stdint.h>

#ifndef BUILD_CONFIG_NAME
#define BUILD_CONFIG_NAME "unknown"
#endif

/* Linker script symbols. _Min_Stack_Size is a size, use its address. */
extern uint8_t _end[];             /* end of .bss, start of the heap */
extern uint8_t _estack[];          /* top of the stack */
extern uint8_t _Min_Stack_Size[];  /* stack size */

/* hw_init.c */
void hw_init(void);
int  board_bt2_pressed(void);

/* leds.c */
void leds_step(void);
void leds_fade_run(void);

#endif /* WOLFDEMO_BOARD_H */
