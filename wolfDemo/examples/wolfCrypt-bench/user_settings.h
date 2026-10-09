/* user_settings.h
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

#ifndef WOLF_USER_SETTINGS_H
#define WOLF_USER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* wolfDemo (STM32U585CI) - wolfSSL user_settings.h                        */
/*                                                                         */
/* Baseline config shared by all build variants. The Makefile selects the  */
/* variant with -D flags:                                                  */
/*   WOLFDEMO_CONFIG_HW - STM32 AES, HASH and PKA, Cortex-M SP math asm    */
/*   WOLFDEMO_BENCH     - per-algorithm heap / stack tracking for bench    */
/*                                                                         */
/* Board: wolfDemo with STM32U585CIT6                                      */
/* UART:  USART1 on PA9 (TX) / PA10 (RX) via CH340G, 115200 8N1           */
/* Clock: HSE 8 MHz -> PLL1 -> 160 MHz SYSCLK                             */
/* ---------------------------------------------------------------------- */

/* Platform / HAL */
#define WOLFSSL_STM32U5
#define WOLFSSL_STM32_CUBEMX
#define STM32_HAL_V2
#define STM32_HAL_TIMEOUT 0xFF

/* The TRNG seeds the DRBG in every variant */
#define STM32_RNG

#ifdef WOLFDEMO_CONFIG_HW
    /* AES (TinyAES IP, no 192-bit keys), HASH and PKA. The application
     * provides the PKA handle (hpka in src/hw_init.c). */
    #define WOLFSSL_STM32_PKA
#else
    /* Pure software crypto for comparison */
    #define NO_STM32_HASH
    #define NO_STM32_CRYPTO
#endif

/* Single-threaded bare-metal, no filesystem */
#define SINGLE_THREADED
#define NO_FILESYSTEM
#define NO_WRITEV
#define NO_MAIN_DRIVER
#define NO_DEV_RANDOM
#define WOLFCRYPT_ONLY
#define WOLFSSL_IGNORE_FILE_WARN
#define SIZEOF_LONG_LONG 8
#define WOLFSSL_GENERAL_ALIGNMENT 4
#define NO_WOLFSSL_SMALL_STACK

/* Time - no RTC, bypass certificate date checking. The benchmark timer is
 * current_time() in src/main_bench.c. */
#define NO_ASN_TIME
#define WOLFSSL_USER_CURRTIME

/* RNG */
#define HAVE_HASHDRBG

/* Math: wolfSSL multi-precision plus single-precision code for the common
 * RSA / DH / ECC key sizes */
#define WOLFSSL_SP_MATH_ALL
#define WOLFSSL_HAVE_SP_RSA
#define WOLFSSL_HAVE_SP_DH
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_384
#define SP_WORD_SIZE 32
#ifdef WOLFDEMO_CONFIG_HW
    #define WOLFSSL_SP_ASM
    #define WOLFSSL_SP_ARM_CORTEX_M_ASM
#endif

/* RSA */
#define WC_RSA_BLINDING
#define WC_RSA_PSS

/* DH */
#define HAVE_DH_DEFAULT_PARAMS
#define HAVE_FFDHE_2048

/* ECC */
#define HAVE_ECC
#define ECC_USER_CURVES
#define HAVE_ECC384
#define ECC_SHAMIR
#define ECC_TIMING_RESISTANT

/* Curve25519 / Ed25519 and Curve448 / Ed448 */
#define HAVE_CURVE25519
#define HAVE_ED25519
#define HAVE_CURVE448
#define HAVE_ED448

/* AES */
#define HAVE_AESGCM
#define GCM_TABLE_4BIT
#define HAVE_AESCCM
#define HAVE_AES_ECB
#define WOLFSSL_AES_DIRECT
#define WOLFSSL_AES_COUNTER
#define WOLFSSL_CMAC

/* ChaCha20-Poly1305 */
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ONE_TIME_AUTH

/* Hashes */
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define WOLFSSL_SHA3
#define WOLFSSL_SHAKE128
#define WOLFSSL_SHAKE256
#define HAVE_HKDF

/* Post-quantum: ML-KEM and ML-DSA. The ML-KEM small memory options cut its
 * heap use to a third at no cost in speed on this part. */
#define WOLFSSL_HAVE_MLKEM
#define WOLFSSL_MLKEM_MAKEKEY_SMALL_MEM
#define WOLFSSL_MLKEM_ENCAPSULATE_SMALL_MEM
#define WOLFSSL_HAVE_MLDSA

/* Drop legacy / unused algorithms */
#define NO_MD4
#define NO_MD5
#define NO_SHA
#define NO_DSA
#define NO_RC4
#define NO_DES3
#define NO_PSK
#define NO_PWDBASED
#define NO_OLD_TLS

/* Test / benchmark */
#define BENCH_EMBEDDED
#define WOLFSSL_BENCH_ECC_ALL
#define USE_CERT_BUFFERS_2048
#define USE_CERT_BUFFERS_256
#define NO_MULTIBYTE_PRINT

#ifdef WOLFDEMO_BENCH
    /* Append "[heap N bytes (N allocs), stack N bytes]" to each benchmark
     * line, used by ../bench_tui.py */
    #define WOLFSSL_TRACK_MEMORY
    #define WOLFSSL_TRACK_MEMORY_VERBOSE
    #define HAVE_STACK_SIZE
    #define HAVE_STACK_SIZE_VERBOSE
#endif

#ifdef __cplusplus
}
#endif

#endif /* WOLF_USER_SETTINGS_H */
