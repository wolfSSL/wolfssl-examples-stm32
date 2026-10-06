/* leds.c
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
 * wolfDemo LED "bark bars" on PB12..PB15 (right to left, active-high).
 *
 * leds_step()     - light the next bar, called for each line of UART output
 * leds_fade_run() - LED-only demo, a software-PWM cross-fade between the bars
 *                   driven by the TIM6 update interrupt. Never returns.
 */

#include "stm32u5xx_hal.h"

#include "board.h"

#define LED_COUNT 4

static const uint16_t led_pins[LED_COUNT] = {
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};

static void led_set(int idx, int on)
{
    HAL_GPIO_WritePin(GPIOB, led_pins[idx], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void leds_step(void)
{
    static int current = LED_COUNT - 1;

    led_set(current, 0);
    current = (current + 1) % LED_COUNT;
    led_set(current, 1);
}

/* Software PWM: 64 brightness levels at ~1 kHz needs a ~64 kHz tick. TIM6
 * counts at 1 MHz and updates every 16 counts (62.5 kHz). */
#define PWM_LEVELS        64u
#define TIM6_COUNT_HZ     1000000u
#define TIM6_PERIOD       16u
/* Brightness changes every N PWM periods */
#define FADE_STEP_PERIODS 2u
#define BRIGHT_MAX        (PWM_LEVELS - 1u)

static TIM_HandleTypeDef htim6;

static volatile uint8_t  pwm_phase;
static volatile uint16_t period_acc;
static volatile uint8_t  lead_led;           /* fading up */
static volatile uint8_t  trail_led;          /* fading down */
static volatile uint8_t  cross;              /* 0..BRIGHT_MAX */
static volatile uint8_t  bright[LED_COUNT];

/* Cross-fade one step: the trailing bar fades down while the leading bar
 * fades up. Once the leading bar is at full brightness it becomes the
 * trailing bar and the next bar starts to fade up. */
static void fade_step(void)
{
    uint8_t i;

    bright[lead_led]  = cross;
    bright[trail_led] = BRIGHT_MAX - cross;
    for (i = 0; i < LED_COUNT; i++) {
        if (i != lead_led && i != trail_led) {
            bright[i] = 0;
        }
    }

    if (cross < BRIGHT_MAX) {
        cross++;
    }
    else {
        cross = 0;
        trail_led = lead_led;
        lead_led = (uint8_t)((lead_led + 1) % LED_COUNT);
    }
}

static void pwm_tick(void)
{
    uint8_t p = pwm_phase;
    uint8_t i;

    for (i = 0; i < LED_COUNT; i++) {
        led_set(i, bright[i] > p);
    }

    p++;
    if (p >= PWM_LEVELS) {
        p = 0;
        if (++period_acc >= FADE_STEP_PERIODS) {
            period_acc = 0;
            fade_step();
        }
    }
    pwm_phase = p;
}

void TIM6_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
        pwm_tick();
    }
}

void leds_fade_run(void)
{
    /* Start with bar 3 fading down and bar 0 fading up */
    lead_led  = 0;
    trail_led = LED_COUNT - 1;
    cross     = 0;
    fade_step();

    /* TIM6 is on APB1, PCLK1 = SYSCLK */
    __HAL_RCC_TIM6_CLK_ENABLE();
    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = (SystemCoreClock / TIM6_COUNT_HZ) - 1u;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = TIM6_PERIOD - 1u;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) == HAL_OK) {
        HAL_NVIC_SetPriority(TIM6_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TIM6_IRQn);
        (void)HAL_TIM_Base_Start_IT(&htim6);
    }

    while (1) {
        __WFI();
    }
}
