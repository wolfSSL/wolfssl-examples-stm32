/* user_settings.h
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* wolfSSL settings for the NUCLEO-C5A3ZG OTA demo: TLS 1.3 client with
 * ML-KEM hybrid key exchange, ECDSA P-256 mutual authentication. */

#ifndef USER_SETTINGS_H
#define USER_SETTINGS_H

/* Platform */
#define WOLFSSL_GENERAL_ALIGNMENT 4
#define SINGLE_THREADED
#define NO_FILESYSTEM
#define NO_WRITEV
#define WOLFSSL_NO_SOCK
#define NO_WOLFSSL_DIR
#define WOLFSSL_USER_IO
#define SIZEOF_LONG_LONG 8
#define WOLFSSL_SMALL_STACK
#define NO_WOLFSSL_SERVER
#define NO_SESSION_CACHE

/* STM32C5 hardware: TinyAES, TRNG, PKA (sign only on C5) */
#ifndef WOLFSSL_STM32C5
    #define WOLFSSL_STM32C5
#endif
#define WOLFSSL_STM32_BARE
#define STM32_CRYPTO
#define STM32_HASH
#define NO_STM32_HMAC
#define STM32_RNG
#define WC_STM32_RNG_CED_DISABLE
#define NO_AES_192
#define WOLFSSL_STM32_PKA
#define HAVE_HASHDRBG
#define NO_DEV_RANDOM

/* Thumb-2 assembly for SHA-2/SHA-3/ChaCha/Poly1305/Curve25519/ML-KEM */
#define WOLFSSL_ARMASM
#define WOLFSSL_ARMASM_THUMB2
#define WOLFSSL_ARMASM_INLINE
#define WOLFSSL_ARMASM_NO_HW_CRYPTO
#define WOLFSSL_ARMASM_NO_NEON
#define WOLFSSL_ARM_ARCH 7
#define WOLFSSL_ARMASM_AES_BLOCK_INLINE

/* SP math with Cortex-M assembly for ECC */
#define WOLFSSL_SP_MATH_ALL
#define WOLFSSL_SP_SMALL
#define SP_WORD_SIZE 32
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_ARM_CORTEX_M_ASM
#define TFM_TIMING_RESISTANT
#define ECC_TIMING_RESISTANT

/* TLS 1.3 only */
#define WOLFSSL_TLS13
#define NO_OLD_TLS
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_HKDF
#define HAVE_AEAD
#define WC_RSA_PSS
#define NO_RSA
#define NO_DH
#define NO_PSK

/* Key exchange: ML-KEM-768 hybrids, plus classical for comparison */
#define WOLFSSL_HAVE_MLKEM
#define WOLFSSL_WC_MLKEM
#define WOLFSSL_PQC_HYBRIDS
#define WOLFSSL_NO_ML_KEM_512
#define WOLFSSL_MLKEM_MAKEKEY_SMALL_MEM
#define WOLFSSL_MLKEM_ENCAPSULATE_SMALL_MEM
#define WOLFSSL_SHA3
#define WOLFSSL_SHAKE128
#define WOLFSSL_SHAKE256
#define HAVE_CURVE25519
#define HAVE_ECC
#define ECC_USER_CURVES
#define HAVE_ECC256
#define ECC_SHAMIR

/* Ciphers / hashes */
#define HAVE_AESGCM
#define GCM_SMALL
#define WOLFSSL_AES_DIRECT
#define HAVE_AES_ECB
#define HAVE_AES_DECRYPT
#define HAVE_CHACHA
#define HAVE_POLY1305
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512

/* Certificates. No RTC: certificate dates are checked against the build
 * time (DEMO_BUILD_EPOCH, set by the Makefile). */
#ifndef __ASSEMBLER__
    #include <time.h>
    time_t demo_time(time_t *t);
    #define XTIME(t) demo_time((t))
#endif
#define WOLFSSL_ASN_TEMPLATE
#define WOLFSSL_SMALL_CERT_VERIFY
#define NO_CHECK_PRIVATE_KEY

/* Trim */
#define NO_DSA
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_DES3
#define NO_PWDBASED
#define NO_ERROR_STRINGS
#define WOLFSSL_NO_FLOAT_FMT

#endif /* USER_SETTINGS_H */
