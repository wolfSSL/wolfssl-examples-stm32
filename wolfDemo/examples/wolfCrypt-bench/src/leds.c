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
 * PB12 drives the smallest bar (one LED) and PB15 the largest (four LEDs).
 *
 * leds_bark()     - one bark, a fade along the bars from the smallest to the
 *                   largest, called after each line of UART output. It
 *                   blocks until the bark is over (~330 ms). The benchmark
 *                   stops its timer before it prints a result, so the bark
 *                   does not affect the results.
 * leds_fade_run() - LED-only demo, a continuous cross-fade between the bars.
 *                   Never returns.
 *
 * Both are a software PWM paced by polling the TIM6 update flag, no
 * interrupts are used. TIM6 only runs while the LEDs are fading.
 */

#include "stm32u5xx_hal.h"

#include "board.h"

#define LED_COUNT 4
#define LED_NONE  0xFFu
#define LED_MASK  (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

static const uint16_t led_pins[LED_COUNT] = {
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};

/* Software PWM: 64 brightness levels at ~1 kHz needs a ~64 kHz tick. TIM6
 * counts at 1 MHz and updates every 16 counts (62.5 kHz). */
#define PWM_LEVELS        64u
#define TIM6_COUNT_HZ     1000000u
#define TIM6_PERIOD       16u
#define BRIGHT_MAX        (PWM_LEVELS - 1u)
/* Brightness changes every N PWM periods (~1 ms each). Each fade from one bar
 * to the next takes PWM_LEVELS steps, so a demo cross-fade takes ~130 ms. A
 * bark is five fades (bar 0 up, three cross-fades, bar 3 down), ~330 ms. */
#define FADE_STEP_PERIODS 2u
#define BARK_STEP_PERIODS 1u

static TIM_HandleTypeDef htim6;
static int timer_ready;

static int      demo;               /* cross-fade forever, not one bark */
static int      running;
static uint8_t  step_periods;
static uint8_t  pwm_phase;
static uint16_t period_acc;
static uint8_t  lead_led;           /* fading up, or LED_NONE */
static uint8_t  trail_led;          /* fading down, or LED_NONE */
static uint8_t  cross;              /* 0..BRIGHT_MAX */
static uint8_t  bright[LED_COUNT];

static int pwm_timer_init(void)
{
    if (timer_ready) {
        return 1;
    }

    /* TIM6 is on APB1, PCLK1 = SYSCLK */
    __HAL_RCC_TIM6_CLK_ENABLE();
    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = (SystemCoreClock / TIM6_COUNT_HZ) - 1u;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = TIM6_PERIOD - 1u;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
        return 0;
    }
    timer_ready = 1;
    return 1;
}

/* Cross-fade one step: the trailing bar fades down while the leading bar
 * fades up. Once the leading bar is at full brightness it becomes the
 * trailing bar and the next bar starts to fade up. The demo wraps around
 * from the largest bar to the smallest, a bark ends once the largest bar has
 * faded out. */
static void fade_step(void)
{
    uint8_t i;

    for (i = 0; i < LED_COUNT; i++) {
        bright[i] = 0;
    }
    if (lead_led != LED_NONE) {
        bright[lead_led] = cross;
    }
    if (trail_led != LED_NONE) {
        bright[trail_led] = (uint8_t)(BRIGHT_MAX - cross);
    }

    if (cross < BRIGHT_MAX) {
        cross++;
        return;
    }

    cross = 0;
    trail_led = lead_led;
    if (demo) {
        lead_led = (uint8_t)((lead_led + 1) % LED_COUNT);
    }
    else if (lead_led == LED_NONE) {
        running = 0;
    }
    else if (lead_led + 1 < LED_COUNT) {
        lead_led++;
    }
    else {
        /* The largest bar fades out with nothing after it */
        lead_led = LED_NONE;
    }
}

static void pwm_tick(void)
{
    uint8_t  p = pwm_phase;
    uint32_t on = 0;
    uint8_t  i;

    for (i = 0; i < LED_COUNT; i++) {
        if (bright[i] > p) {
            on |= led_pins[i];
        }
    }
    /* Set the lit bars and reset the others in one write */
    GPIOB->BSRR = on | ((LED_MASK & ~on) << 16);

    p++;
    if (p >= PWM_LEVELS) {
        p = 0;
        if (++period_acc >= step_periods) {
            period_acc = 0;
            fade_step();
        }
    }
    pwm_phase = p;
}

/* Run the software PWM, one tick per TIM6 update, until the fade ends. Then
 * stop TIM6 and turn all of the bars off. */
static void pwm_run(void)
{
    pwm_phase  = 0;
    period_acc = 0;
    running    = 1;
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE(&htim6);

    while (running) {
        while (!__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE)) {
        }
        __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
        pwm_tick();
    }

    __HAL_TIM_DISABLE(&htim6);
    GPIOB->BSRR = (uint32_t)LED_MASK << 16;
}

void leds_bark(void)
{
    uint8_t i;

    if (!pwm_timer_init()) {
        return;
    }

    /* Start with the smallest bar fading up and nothing fading down */
    for (i = 0; i < LED_COUNT; i++) {
        bright[i] = 0;
    }
    lead_led     = 0;
    trail_led    = LED_NONE;
    cross        = 0;
    step_periods = BARK_STEP_PERIODS;
    demo         = 0;
    pwm_run();
}

void leds_fade_run(void)
{
    if (pwm_timer_init()) {
        /* Start with bar 3 fading down and bar 0 fading up */
        lead_led     = 0;
        trail_led    = LED_COUNT - 1;
        cross        = 0;
        step_periods = FADE_STEP_PERIODS;
        demo         = 1;
        fade_step();
        pwm_run();
    }

    while (1) {
        __WFI();
    }
}
