/* main_bench.c
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
 * Runs the wolfCrypt benchmark on the wolfDemo board in a loop, with a pause
 * between runs. Output goes to USART1 (the USB-C port) and each line steps
 * the LED bars. Hold BT2 during reset for the LED-only demo instead.
 */

#include "stm32u5xx_hal.h"
#include <stdio.h>

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/mem_track.h"
#include "wolfcrypt/benchmark/benchmark.h"

#include "board.h"

/* Pause between benchmark runs */
#define BENCH_RUN_DELAY_MS 5000

#ifdef WOLFDEMO_CONFIG_HW
    #define BENCH_ACCEL_DESC "hardware accelerated"
#else
    #define BENCH_ACCEL_DESC "software only"
#endif

/* current_time stub for benchmark (WOLFSSL_USER_CURRTIME) */
double current_time(int reset)
{
    (void)reset;
    return (double)HAL_GetTick() / 1000.0;
}

int main(void)
{
    unsigned long run = 0;

    hw_init();

    if (board_bt2_pressed()) {
        leds_fade_run();
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#ifdef HAVE_STACK_SIZE_VERBOSE
    /* Fill the stack with a known pattern so the benchmark can report the
     * peak stack use of each algorithm */
    STACK_SIZE_CHECK_INIT_TOP(_estack, (size_t)_Min_Stack_Size);
#endif

    while (1) {
        run++;

        /* Clear the terminal and print the banner */
        printf("\x1b[2J\x1b[H");
        printf("\x1b[4m\x1b[97mwolf\x1b[96mSSL \x1b[38;2;255;210;0m"
               "STM32U585 @ %lu MHz, " BENCH_ACCEL_DESC "\x1b[0m\n",
               (unsigned long)(HAL_RCC_GetSysClockFreq() / 1000000u));
        printf("wolfSSL %s (CONFIG=%s), benchmark run %lu\n",
               LIBWOLFSSL_VERSION_STRING, BUILD_CONFIG_NAME, run);

        (void)benchmark_test(NULL);

        HAL_Delay(BENCH_RUN_DELAY_MS);
    }
}
