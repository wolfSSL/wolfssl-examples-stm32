/* stm32u5xx_it.c
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

/* Minimal interrupt handlers for STM32U585. TIM6_IRQHandler is in leds.c. */
#include "stm32u5xx_hal.h"

void NMI_Handler(void)           { }
void HardFault_Handler(void)     { while (1) { } }
void MemManage_Handler(void)     { while (1) { } }
void BusFault_Handler(void)      { while (1) { } }
void UsageFault_Handler(void)    { while (1) { } }
void SecureFault_Handler(void)   { while (1) { } }
void SVC_Handler(void)           { }
void DebugMon_Handler(void)      { }
void PendSV_Handler(void)        { }
void SysTick_Handler(void)       { HAL_IncTick(); }
