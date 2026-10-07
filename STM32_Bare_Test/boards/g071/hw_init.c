/* hw_init.c - STM32G071RB (NUCLEO-G071RB), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-G071RB:
 *   - HSI16 (16 MHz) as SYSCLK by default; STM32_BARE_CLK_HZ=64000000
 *     (make CLK=64) brings up the PLL for the part's 64 MHz maximum.
 *   - USART2 on PA2 (TX) / PA3 (RX) AF1, 115200 8N1, ST-LINK V2-1 VCP.
 *   - Cortex-M0+. No FPU.
 *
 * G071 silicon HW crypto: NONE. The G071 sub-family has no AES, no
 * HASH, no RNG, no PKA peripheral (see stm32g071xx.h: only RNG_TypeDef-
 * adjacent symbols are notably absent). All cryptography falls back to
 * software, including DRBG -- HASH_DRBG seeded by HW timer entropy.
 * This board exercises the pure-SW path on a Cortex-M0+ (smaller test
 * footprint via STM32_G071_TRIM, modeled on STM32_U083_TRIM).
 */

#include "stm32g0xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* Makefile CLK= sets this; default is the post-reset HSI16. */
#ifndef STM32_BARE_CLK_HZ
#define STM32_BARE_CLK_HZ 16000000
#endif
#define G071_SYSCLK_HZ ((uint32_t)STM32_BARE_CLK_HZ)

/* ---- printf retarget over USART2 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART2->TDR = (uint32_t)ch & 0xFFu;
}


static void clock_init(void)
{
    /* After reset: HSION=1, HSIRDY=1, SW=HSI16. */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

#if STM32_BARE_CLK_HZ == 64000000
    /* 64 MHz needs 2 flash wait states (RM0444: 1 WS above 24 MHz,
     * 2 WS above 48 MHz). Raise latency before raising the clock. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) |
                 (2u << FLASH_ACR_LATENCY_Pos) |
                 FLASH_ACR_PRFTEN | FLASH_ACR_ICEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) !=
           (2u << FLASH_ACR_LATENCY_Pos)) { }

    /* PLL off before reconfiguring. */
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    /* HSI16 / M=1 * N=8 / R=2 = 64 MHz. VCO at 128 MHz is inside the
     * 64-344 MHz range. PLLR is encoded as the divider minus one. */
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSI |
                   (0u << RCC_PLLCFGR_PLLM_Pos) |
                   (8u << RCC_PLLCFGR_PLLN_Pos) |
                   (1u << RCC_PLLCFGR_PLLR_Pos) |
                   RCC_PLLCFGR_PLLREN;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    /* Switch SYSCLK to PLLRCLK (SW = 0b010) and wait for SWS to follow. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (2u << RCC_CFGR_SW_Pos);
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (2u << RCC_CFGR_SWS_Pos)) { }
#endif
}

static void uart_init(void)
{
    /* Enable GPIOA clock (IOPENR bit GPIOAEN) */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    (void)RCC->IOPENR;

    /* PA2 (TX), PA3 (RX): MODER = AF (10b), AF1 (USART2) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);

    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);

    /* AFRL: PA2 -> AFR[0] bits[11:8]; PA3 -> AFR[0] bits[15:12]; AF1 = 0x1 */
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (1u << GPIO_AFRL_AFSEL2_Pos) |
                     (1u << GPIO_AFRL_AFSEL3_Pos);

    /* Enable USART2 clock (APBENR1 bit USART2EN) */
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    (void)RCC->APBENR1;

    /* USART2: 8N1, oversampling 16. PCLK = HCLK = SYSCLK. */
    USART2->CR1 = 0;
    USART2->BRR = G071_SYSCLK_HZ / 115200u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(G071_SYSCLK_HZ);
}

uint32_t board_sysclk_hz(void)
{
    return G071_SYSCLK_HZ;
}


const char *board_name(void)
{
    return "NUCLEO-G071RB";
}
