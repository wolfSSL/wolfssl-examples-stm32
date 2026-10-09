/* hw_init.c
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

/*
 * HW init for the wolfDemo board (STM32U585CIT6).
 *
 * Clock:   HSE 8 MHz -> PLL1 (M=1, N=20, R=1) -> 160 MHz SYSCLK
 *          HSI48 is the RNG kernel clock
 * UART:    USART1 on PA9 (TX) / PA10 (RX) via the CH340G, 115200 8N1
 * LEDs:    PB12, PB13, PB14, PB15 (right to left), active-high
 * Buttons: BT1 on PB4, BT2 on PB5, active-low with external pull-ups
 * Crypto:  AES, HASH, PKA and RNG clocks are enabled from the HAL MSP
 *          callbacks below when wolfCrypt initialises each peripheral
 */

#include "stm32u5xx_hal.h"
#include <stdio.h>

#include "board.h"

UART_HandleTypeDef huart1;

#ifdef WOLFDEMO_CONFIG_HW
/* The wolfSSL STM32 port (CubeMX build) uses this PKA handle for ECDSA sign
 * and verify. It initialises the PKA on first use. */
PKA_HandleTypeDef hpka = { .Instance = PKA };
#endif

static void Error_Handler(void)
{
    __disable_irq();
    while (1) { __NOP(); }
}

/* UART printf retarget. Each '\n' is sent as "\r\n" for serial terminals and
 * moves the LED bars on one step, so the LEDs track the benchmark output. */
int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;

    HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
    return ch;
}

int _write(int file, char *ptr, int len)
{
    int i;

    (void)file;
    for (i = 0; i < len; i++) {
        if (ptr[i] == '\n') {
            __io_putchar('\r');
            leds_step();
        }
        __io_putchar(ptr[i]);
    }
    return len;
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1)
            != HAL_OK) {
        Error_Handler();
    }

    /* HSE 8 MHz -> PLL1 -> 160 MHz, HSI48 for the RNG */
    osc.OscillatorType  = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    osc.HSEState        = RCC_HSE_ON;
    osc.HSI48State      = RCC_HSI48_ON;
    osc.PLL.PLLState    = RCC_PLL_ON;
    osc.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMBOOST   = RCC_PLLMBOOST_DIV1;
    osc.PLL.PLLM        = 1;
    osc.PLL.PLLN        = 20;
    osc.PLL.PLLP        = 2;
    osc.PLL.PLLQ        = 2;
    osc.PLL.PLLR        = 1;
    osc.PLL.PLLRGE      = RCC_PLLVCIRANGE_1;  /* 8-16 MHz input range */
    osc.PLL.PLLFRACN    = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    /* HCLK = PCLK1 = PCLK2 = PCLK3 = 160 MHz */
    clk.ClockType       = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                        | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider   = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider  = RCC_HCLK_DIV1;
    clk.APB2CLKDivider  = RCC_HCLK_DIV1;
    clk.APB3CLKDivider  = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* LED bars off */
    HAL_GPIO_WritePin(GPIOB,
        GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);
    gpio.Pin   = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* BT1 and BT2 */
    gpio.Pin   = GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void USART1_Init(void)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

void hw_init(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Instruction cache for code in flash */
    if (HAL_ICACHE_Enable() != HAL_OK) {
        Error_Handler();
    }

    GPIO_Init();
    USART1_Init();
}

int board_bt2_pressed(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET;
}

/* HAL MSP callbacks for USART1 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance == USART1) {
        /* Kernel clock is PCLK2 (reset default) */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* USART1: PA9 = TX, PA10 = RX, AF7 */
        gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
}

/* HAL MSP callback for the TRNG. wolfCrypt initialises and de-initialises
 * the RNG around each seed request. There is no MspDeInit so the clock keeps
 * running, the PKA needs it (see HAL_PKA_MspInit). Kernel clock is HSI48
 * (reset default). */
void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng)
{
    (void)hrng;
    __HAL_RCC_RNG_CLK_ENABLE();
}

#ifdef WOLFDEMO_CONFIG_HW
/* HAL MSP callbacks for the crypto peripherals (clock enable / disable) */
void HAL_CRYP_MspInit(CRYP_HandleTypeDef *hcryp)
{
    (void)hcryp;
    __HAL_RCC_AES_CLK_ENABLE();
}

void HAL_CRYP_MspDeInit(CRYP_HandleTypeDef *hcryp)
{
    (void)hcryp;
    __HAL_RCC_AES_CLK_DISABLE();
}

void HAL_HASH_MspInit(HASH_HandleTypeDef *hhash)
{
    (void)hhash;
    __HAL_RCC_HASH_CLK_ENABLE();
}

void HAL_HASH_MspDeInit(HASH_HandleTypeDef *hhash)
{
    (void)hhash;
    __HAL_RCC_HASH_CLK_DISABLE();
}

void HAL_PKA_MspInit(PKA_HandleTypeDef *hpkah)
{
    (void)hpkah;
    /* The PKA never reports INITOK unless the RNG is clocked */
    __HAL_RCC_RNG_CLK_ENABLE();
    __HAL_RCC_PKA_CLK_ENABLE();
}

void HAL_PKA_MspDeInit(PKA_HandleTypeDef *hpkah)
{
    (void)hpkah;
    __HAL_RCC_PKA_CLK_DISABLE();
}
#endif /* WOLFDEMO_CONFIG_HW */

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("ASSERT: %s:%lu\n", (char *)file, (unsigned long)line);
    Error_Handler();
}
#endif
