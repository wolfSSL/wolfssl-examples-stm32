/* main_test.c
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
 * Runs the wolfCrypt self-test once on the wolfDemo board. Output goes to
 * USART1 (the USB-C port).
 */

#include "stm32u5xx_hal.h"
#include <stdio.h>

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfcrypt/test/test.h"

#include "board.h"

int main(void)
{
    int ret;
    int cleanup_ret;

    hw_init();

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("\n");
    printf("========================================\n");
    printf("wolfCrypt test - wolfDemo STM32U585 (CONFIG=%s)\n",
           BUILD_CONFIG_NAME);
    printf("wolfSSL version: %s\n", LIBWOLFSSL_VERSION_STRING);
    printf("SYSCLK: %lu Hz\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    printf("========================================\n\n");

    ret = wolfCrypt_Init();
    if (ret != 0) {
        printf("wolfCrypt_Init failed: %d\n", ret);
        while (1) { __NOP(); }
    }

    ret = (int)wolfcrypt_test(NULL);

    cleanup_ret = wolfCrypt_Cleanup();
    if (cleanup_ret != 0) {
        printf("wolfCrypt_Cleanup failed: %d\n", cleanup_ret);
        if (ret == 0) {
            ret = cleanup_ret;
        }
    }

    printf("\nTest result: %d (%s)\n", ret, ret == 0 ? "PASS" : "FAIL");

    while (1) { __NOP(); }
    return ret;
}
