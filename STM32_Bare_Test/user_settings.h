/* user_settings.h
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Multi-board wolfSSL config for STM32_Bare_Test.
 *
 * Board selected via -DSTM32_BOARD_<NAME> from the Makefile.
 * Build flavor selected via -DBUILD_BARE / -DBUILD_C from the Makefile.
 */

#ifndef WOLF_USER_SETTINGS_H
#define WOLF_USER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Board selection                                                        */
/* ---------------------------------------------------------------------- */
#if defined(STM32_BOARD_H573)
    #define WOLFSSL_STM32H5
    /* NUCLEO-H573ZI: Cortex-M33 with full crypto (TinyAES + HASH +
     * RNG + SAES + PKA V2 + DHUK). Distinct from H563 which only has
     * HASH + RNG + PKA. Same Nucleo-144 PCB as H563ZI (USART3
     * PD8/PD9 AF7 to ST-LINK VCP). TZEN should be 0 from factory
     * for this dev kit; if a future H573 ships TZEN=1 the option
     * byte needs to be flipped before running NS code. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_H5)
    #define WOLFSSL_STM32H5
    /* H563 has HASH + RNG + PKA only (no AES peripheral). H573 + H533 do
     * have AES; switching to one of those boards needs a different STM32
     * board flag. PKA is the V2 IP. */
    #define NO_STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    /* H563 has the reduced "light" PKA: ECDSA VERIFY runs in HW but the
     * silicon cannot SIGN (mode 0x24 aborts with OPERRF; confirmed on two
     * boards, bare + HAL, NS + TZEN=1 secure -- see /tmp ST report). ST
     * confirms this is by design: H563 is verify-only, H573 is full
     * sign+verify. Enabling WOLFSSL_STM32_PKA auto-selects
     * WC_STM32_PKA_VERIFY_ONLY for STM32H563xx (see port/st/stm32.h):
     * ECDSA verify -> HW PKA, ECDSA sign -> software. */
    #define WOLFSSL_STM32_PKA
#elif defined(STM32_BOARD_H7)
    #define WOLFSSL_STM32H7
    /* H753 has CRYP + HASH + RNG (CRYP IP, FIFO-based, like F4/F7) + SAES */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    /* CRYP IP supports 128/192/256-bit AES keys */
#elif defined(STM32_BOARD_U5)
    #define WOLFSSL_STM32U5
    /* U575 has HASH + RNG only -- NO AES, NO PKA. U585+ has AES + PKA;
     * switching to U585 needs the chip-specific board flag. */
    #define NO_STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_U3)
    #define WOLFSSL_STM32U3
    /* U385 has AES + HASH + RNG + PKA (TinyAES IP) + SAES + DHUK.
     * Both DHUK directions are validated on this chip from NS state:
     * Wrap (KEYSEL=HW, MODE=ENCRYPT, deterministic chip-bound output)
     * and the wrapped-key unwrap (MODE=KEYDERIVATION then MODE=DECRYPT),
     * which signals completion via CCF with SR.KEYVALID confirming the
     * loaded key. The two ARE inverses: a blob wrapped in
     * WC_STM32_WRAP_ORDER_RAW unwraps back to the key it wrapped.
     * Earlier notes here claimed the unwrap needed secure state and was
     * not an inverse; both were driver bugs, since fixed.
     * WOLFSSL_DHUK is still not enabled by default here; build with
     * -DWOLFSSL_DHUK (TARGET=dhuk does this) to exercise it. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE)
        /* Both build paths: wolfssl's stm32.c now lists WOLFSSL_STM32U3 in
         * the PKA hal_pka.h include-chain, so the HAL build gets HW PKA too
         * (needed for DHUK seed-wrapped ECDSA sign under CUBEMX). The app
         * supplies the PKA handle -- see boards/u3/hw_init_cubemx.c. */
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_U585)
    #define WOLFSSL_STM32U5
    /* U585 has AES + HASH + RNG + SAES + PKA (V2 layout, TinyAES IP) */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_U545)
    #define WOLFSSL_STM32U5
    /* U545 has AES + HASH + RNG + SAES + PKA (V2 layout, TinyAES IP) -
     * same crypto IP set as U585, lower-power sub-family. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_C5A3)
    #define WOLFSSL_STM32C5
    /* C5A3 has TinyAES + HASH + RNG + SAES + PKA (V2 layout, all on AHB2).
     * Same crypto IP set as U585/U545 (TinyAES) plus the new-gen HASH IP.
     * RNG: the earlier "CONDRST/SR.BUSY stuck" was NOT a silicon fault --
     * the RNG had no kernel clock. The C5 RNG runs off CK48
     * (RCC_CCIPR2.CK48SEL), whose reset default is NONE; nothing selected a
     * source, so conditioning stalled with no clock (CECS=0 because there are
     * no clock edges to flag). boards/c5a3/hw_init.c now sources CK48 from
     * HSIDIV3 (HSI/3 = 48 MHz, internal); with that the RNG conditions
     * immediately and returns entropy (validated on silicon). HW RNG enabled. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define NO_STM32_HMAC  /* HW HASH state machine doesn't survive HKDF */
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    /* HW PKA: the C5 protected ECDSA SIGN now works in wolfSSL (mode 0x24 armed
     * before operands via wc_stm32_pka_arm_mode in stm32.c, SIGN path only;
     * validated R/S=OK vs ST's NIST P-256 CAVP vector, TARGET=c5sign). The plain
     * ECDSA VERIFY (0x26, confirmed a non-protected op == U5's verify) still
     * returns OUT_RESULT=0 in the wolfSSL context, on the standard path, even
     * though the bare probe verifies the identical operands OK -- an unresolved
     * context bug. Since verify is PUBLIC, HW PKA runs sign-only here: enabling
     * WOLFSSL_STM32_PKA on the C5 auto-selects WC_STM32_PKA_SIGN_ONLY (see
     * port/st/stm32.h), so HW signs and software verifies. */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_V8)
    /* NUCLEO-V873XJ (STM32V873XJ): Cortex-M85, Armv8.1-M + Helium, 4 MB
     * flash, device ID 0x499 Rev B.
     *
     * Inventory read from STM32CubeProgrammer 2.22.0's SVD/STM32V873.svd:
     * CRYP (fat -- K0LR..K3RR, CSGCMCCM0..7R, GCM_CCMPH, 2-bit KEYSIZE, so
     * AES-192 IS supported), SAES (DHUK/BHK via CR.KEYSEL), HASH (new-gen,
     * CSR0..102 -> HASH_CR_SIZE 103, HR0..15 -> 64-byte digest), PKA (V2
     * with BOTH sign 0x24 and verify 0x26 -- not restricted like C5/H563),
     * RNG (CONDRST/NISTC/HTCR0..3) and CCB. All six clocks live on
     * RCC_AHB3ENR bits 0/1/2/3/8/15. That topology matches N6, so the
     * wolfSSL stm32.h arms should be cloned from N6, not from C5.
     *
     * HASH_CR.ALGO is 4 bits with 13 values. Seven -- 0x0/0x2/0x3/0xC/0xD/
     * 0xE/0xF -- match wolfSSL's existing new-gen SHA-1/2 mapping exactly.
     * The six extras (0x4-0x7, 0x8-0x9) are almost certainly SHA3-224/256/
     * 384/512 + SHAKE128/256, but the SVD enum names are placeholders so
     * that assignment is INFERRED, not documented -- confirm against the RM
     * or an on-silicon KAT before relying on it. Note there is no SHA3CFGR
     * register and the digest stops at 64 bytes, which strongly suggests a
     * fixed-length (non-resumable) XOF squeeze -- i.e. no ML-KEM/ML-DSA win.
     *
     * HW blocks are enabled one at a time as each is validated on this
     * silicon. */
    #define WOLFSSL_STM32V8
    /* Umbrella derives STM32_RNG/HASH/CRYPTO/HMAC; opt out of what is not
     * validated yet. RNG's MCG48 source is enabled in hw_init.c (V8 has no
     * CCIPR mux for it). */
    /* AES enabled 8/26, routed through the SAES instance like N6: the BARE
     * TinyAES driver shape matches SAES registers (IVR0..3, KEYR0..7); the
     * fat CRYP (IV0LR.., K0LR..) would need its own bare path wiring. Costs
     * AES-192 (settings.h gates NO_AES_192 on USE_SAES for V8, as for N6). */
    #define WOLFSSL_STM32_USE_SAES
    /* V2 PKA (sign AND verify, per PKA_CR.MODE enum). Operand RAM offsets
     * are IP constants emitted by svd2cmsis.py. BARE only, as on N657. */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
    #define NO_STM32_HMAC  /* match C5A3/C562 until HKDF-over-HW is proven */
#elif defined(STM32_BOARD_C562)
    #define WOLFSSL_STM32C5
    /* NUCLEO-C562RE: same C5 family as C5A3. Same crypto IP set: AES
     * (CRYP-shape) + HASH + RNG on AHB2. The earlier "RNG CONDRST-stuck"
     * was not a silicon fault -- the RNG had no kernel clock (CK48 mux
     * defaulted to NONE). boards/c562/hw_init.c now sources CK48 from
     * HSIDIV3 (HSI/3 = 48 MHz), the same fix validated on the C5A3, so
     * HW RNG is enabled here too. NOT re-validated on hardware (no C562 on
     * the bench); flip back to NO_STM32_RNG if it regresses on silicon.
     * HW PKA stays off (the C5 protected-mode PKA<->RNG path is open work,
     * see the C5A3 arm). */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define NO_STM32_HMAC  /* HW HASH state machine doesn't survive HKDF */
    #define STM32_RNG
    #define NO_AES_192     /* TinyAES does not support 192-bit keys */
#elif defined(STM32_BOARD_F767)
    #define WOLFSSL_STM32F7
    /* STM32F767ZI silicon has RNG only -- NO CRYP, NO HASH peripheral.
     * Only F777xx and F779xx variants of the F7 line have HASH + CRYP.
     * BARE on F767 therefore only accelerates RNG; AES/SHA fall back
     * to software (or thumb2 ASM if CONFIG=asm). */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_N657)
    #define WOLFSSL_STM32N6
    /* N657 silicon has CRYP + HASH + RNG + SAES + PKA, all on AHB3.
     *   HASH : new-gen IP (4-bit ALGO, same shape as H5/U3/C5). Enabled.
     *   RNG  : standard STM32 RNG. Enabled.
     *   AES  : routed through SAES via WOLFSSL_STM32_USE_SAES. SAES on
     *          N6 has the TinyAES-shape register layout (CR.EN bit 0,
     *          DATATYPE, MODE, CHMOD, KEYR0..7, IVR0..3, DINR, DOUTR,
     *          ISR/ICR) -- matches the BARE TinyAES driver in stm32.c.
     *          The older fat CRYP IP is not wired up (needs its own
     *          register-level driver; see RCC_AHB3ENR_CRYPEN arm).
     *   PKA  : V2 layout. Enabled and validated on silicon -- ECC_MUL
     *          (mode 0x20), ECDSA sign (0x24) and verify (0x26) all
     *          complete on HW (the earlier ECC_MUL-timeout note was
     *          stale; WC_STM32_PKA_DIAG confirms the op modes).
     * Runs the BARE app from AXISRAM2 (no internal flash) at 600 MHz
     * CPU. */
    /* AES routed to the SAES instance (same TinyAES-shape register path
     * validated on H7S3). The N6 "fat" CRYP is in the security domain and
     * times out from NS code, so SAES is the NS-accessible AES IP. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define WOLFSSL_STM32_USE_SAES   /* route TinyAES driver to SAES */
    #define NO_AES_192               /* TinyAES does not support 192-bit */
    #if defined(BUILD_BARE)
        /* V2 PKA validated on N6 silicon: ECC_MUL (mode 0x20), ECDSA sign
         * (0x24) and verify (0x26) all complete on HW. The earlier
         * ECC_MUL-timeout note was stale. */
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_WB55)
    #define WOLFSSL_STM32WB
    /* WB55 has AES1 (TinyAES) + RNG + PKA, NO HASH peripheral. PKA on
     * WB55 is the V1 IP (single ECC scalar mul; no coefB/primeOrder). */
    #define STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE) && !defined(STM32_BUILD_CUBEMX)
        /* wolfssl's stm32.c V1 PKA on WB hits the same RSign/SSign
         * field-name mismatch as WL55 under cubemx; ECC falls back
         * to SW for cubemx until the upstream port handles it. */
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_WL55)
    #define WOLFSSL_STM32WL
    /* WL55JC: dual-core M4 + M0+, sub-GHz radio. Crypto on the M4 side
     * is TinyAES + RNG + PKA (V1 layout, same as WB55), no HASH. WL has
     * tight 64K SRAM so we run minimal wolfcrypt -- bench only. RNG
     * kernel clock is routed to MSI 4 MHz in board hw_init (the default
     * RNGSEL = PLL Q has no clock); BARE wc_GenerateSeed handles
     * SECS/CECS recovery so the IP stays usable under sustained load. */
    #define STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE) && !defined(STM32_BUILD_CUBEMX)
        /* wolfssl's stm32.c V1 PKA type definitions on WL (e.g.
         * PKA_ECDSASignOutTypeDef.RSign/SSign) differ from what the
         * shared port code expects; build hits ~5 type-mismatch
         * errors. Limit PKA to BARE for now -- cubemx falls back to
         * SW ECC. */
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_G491)
    #define WOLFSSL_STM32G4
    /* STM32G491RE has RNG + PKA but NO AES and NO HASH peripheral.
     * (The AES block is only on the G4A1xx variant in this G4 sub-family.)
     * BARE on G491 therefore only accelerates RNG (and PKA, when the
     * bare-metal PKA driver lands). AES/SHA fall back to software. */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_G474)
    #define WOLFSSL_STM32G4
    /* STM32G474RE has RNG + PKA but NO AES and NO HASH peripheral.
     * G474 is a non-crypto G4 variant; the AES IP lives on the crypto-
     * enabled G4 variants only (G414 / G441 / G483 / G484 / G4A1). BARE
     * on G474 therefore accelerates RNG + ECC (via V1 PKA) only; AES/SHA
     * fall back to software. The wolfssl-side WOLFSSL_STM32G4 arm
     * already wires AES via RCC_AHB2ENR_AESEN -- a future G484 / G4A1
     * board would pick that up automatically. */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_WBA52)
    #define WOLFSSL_STM32WBA
    /* WBA52 has TinyAES + HASH + RNG + PKA + SAES (V2 PKA layout). */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_F207)
    #define WOLFSSL_STM32F2
    /* STM32F207ZG silicon has RNG only -- NO CRYP, NO HASH peripheral.
     * (CRYP+HASH are only on F215xx/F217xx in the F2 line.) BARE on
     * F207 therefore only accelerates RNG; AES/SHA fall back to
     * software (or thumb2 ASM if CONFIG=asm). */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_F303)
    #define WOLFSSL_STM32F3
    /* STM32F303xE has NO HW crypto -- no CRYP, no HASH, no TRNG.
     * Pure software wolfCrypt; thumb2 inline-asm is available under
     * CONFIG=asm. */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define NO_STM32_RNG
#elif defined(STM32_BOARD_F437)
    #define WOLFSSL_STM32F4
    #define STM32_HASH
    #define STM32_CRYPTO
    #define STM32_RNG
    /* F4 CRYP peripheral does not support 192-bit keys */
    #define NO_AES_192
#elif defined(STM32_BOARD_F439)
    #define WOLFSSL_STM32F4
    #define STM32_HASH
    #define STM32_CRYPTO
    #define STM32_RNG
    /* F4 CRYP peripheral does not support 192-bit keys */
    #define NO_AES_192
#elif defined(STM32_BOARD_L4A6)
    #define WOLFSSL_STM32L4
    /* STM32L4A6ZG silicon has AES + HASH + RNG (TinyAES IP shape on
     * AHB2). No PKA, no SAES. First L4-family entry in the BARE matrix. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
#elif defined(STM32_BOARD_H723)
    #define WOLFSSL_STM32H7
    /* STM32H723ZG silicon has RNG only -- the H72x sub-family does not
     * include CRYP/HASH (only H7A3/H7B3 do). BARE on H723 therefore
     * only accelerates RNG; AES/SHA fall back to software. */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_H7A3)
    #define WOLFSSL_STM32H7
    /* STM32H7A3ZI-Q silicon has RNG only. The "value-line" H7A3/H7B3
     * sub-family swapped the classic H7's CRYP/HASH IPs for larger
     * flash/SRAM and DSI peripherals -- only RNG remains. Same surface
     * as the H723 arm; AES/SHA fall back to software. */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_H7S3)
    /* NUCLEO-H7S3L8 (Cortex-M7, H7RS family). Silicon HW crypto:
     * "fat" CRYP (same shape as H753) + classic HASH (same shape as
     * H753) + RNG + SAES (TinyAES-shape) + V2 PKA. All on AHB3ENR (vs
     * classic H7 which has CRYP/HASH/RNG on AHB2). First H7-family port
     * with V2 PKA + SAES.
     *
     * AES routing: ST's H7S Cube examples drive AES exclusively via the
     * SAES instance -- the plain CRYP is in the security domain by
     * default and times out from non-secure code. We therefore route
     * the BARE TinyAES driver to SAES via WOLFSSL_STM32_USE_SAES.
     * SAES on H7S has the same register layout as on U5/H5/WBA/C5 so
     * the existing TinyAES code path is reused unmodified.
     */
    #define WOLFSSL_STM32H7S
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define WOLFSSL_STM32_USE_SAES   /* route TinyAES driver to SAES */
    #define NO_AES_192               /* TinyAES does not support 192-bit */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_L552)
    #define WOLFSSL_STM32L5
    /* L552ZE has HASH + RNG only -- NO AES, NO PKA. L562 has the
     * full set (TinyAES + PKA + SAES); switching to L562 needs the
     * chip-specific board flag. TrustZone is disabled at option-byte
     * level for this BARE build (TZEN=0xC3). */
    #define NO_STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
#elif defined(STM32_BOARD_L562)
    #define WOLFSSL_STM32L5
    /* L562 has TinyAES + HASH + RNG + PKA (V1) + SAES + DHUK. The
     * L562E-DK on the bench ships with TZEN=0 so the BARE binary
     * runs from 0x08000000 in non-secure state directly. */
    #define STM32_CRYPTO
    #define STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    #if defined(BUILD_BARE)
        #define WOLFSSL_STM32_PKA
    #endif
#elif defined(STM32_BOARD_U083)
    #define WOLFSSL_STM32U0
    /* NUCLEO-U083RC: Cortex-M0+, 256 KB flash, 40 KB SRAM. TinyAES IP
     * + RNG only. No SAES, no HASH, no PKA, no CRYP. Lowest-end board
     * in the matrix -- AES is the only HW accelerator. */
    #define STM32_CRYPTO
    #define NO_STM32_HASH
    #define STM32_RNG
    #define NO_AES_192   /* TinyAES does not support 192-bit keys */
    /* U0 is 256 KB flash / 40 KB SRAM. Default user_settings (P-384,
     * P-521, Curve25519, Ed25519, SHA-384/512) overflows by ~28 KB,
     * so drop the heavy-tail algorithms and large math sizes. Keep
     * AES + SHA-256 + P-256 + ChaCha/Poly + HKDF as a minimal TLS-
     * facing surface. */
    #define STM32_U083_TRIM
    #define WOLFSSL_SMALL_STACK
#elif defined(STM32_BOARD_G071)
    #define WOLFSSL_STM32G0
    /* NUCLEO-G071RB: Cortex-M0+, 128 KB flash, 36 KB SRAM. NO HW
     * crypto IPs at all -- no AES, no HASH, no RNG, no PKA. Everything
     * runs in software. Smaller flash than U083 (128 KB vs 256 KB) so
     * the same trim set as U083 is applied. HASH_DRBG is seeded from
     * the BARE SysTick + boot-jitter entropy fallback (no HW RNG). */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define NO_STM32_RNG
    #define STM32_G071_TRIM
    #define WOLFSSL_SMALL_STACK
#elif defined(STM32_BOARD_C031)
    #define WOLFSSL_STM32C0
    /* NUCLEO-C031C6: Cortex-M0+, 32 KB flash, 12 KB SRAM. The smallest
     * MCU in the matrix. ZERO HW crypto. The wolfcrypt_test suite alone
     * is ~80 KB+ even when stripped to the bone -- it cannot fit. The
     * C031_TRIM disables RUN_WOLFCRYPT_TEST_SUITE and trims the SW path
     * to only the algorithms exercised by main_test.c's KAT block:
     * SHA-256, AES-CBC/ECB, ChaCha20-Poly1305. Even that is tight. */
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define NO_STM32_RNG
    #define STM32_C031_TRIM
    #define WOLFSSL_SMALL_STACK
#else
    #error "Define one of STM32_BOARD_C031 / STM32_BOARD_H5 / STM32_BOARD_H573 / STM32_BOARD_H7 / STM32_BOARD_H723 / STM32_BOARD_H7A3 / STM32_BOARD_H7S3 / STM32_BOARD_U5 / STM32_BOARD_U3 / STM32_BOARD_U585 / STM32_BOARD_U545 / STM32_BOARD_U083 / STM32_BOARD_C562 / STM32_BOARD_C5A3 / STM32_BOARD_WB55 / STM32_BOARD_WL55 / STM32_BOARD_G071 / STM32_BOARD_G474 / STM32_BOARD_G491 / STM32_BOARD_WBA52 / STM32_BOARD_F207 / STM32_BOARD_F303 / STM32_BOARD_F437 / STM32_BOARD_F439 / STM32_BOARD_F767 / STM32_BOARD_L4A6 / STM32_BOARD_L552 / STM32_BOARD_L562 / STM32_BOARD_V8"
#endif

/* ---------------------------------------------------------------------- */
/* Build flavor                                                           */
/* ---------------------------------------------------------------------- */
#if defined(BUILD_BARE)
    /* CONFIG=bare drives wolfcrypt through the STM32 HW IP blocks. Two
     * BUILD axes select HOW: BUILD=bare uses the direct-register
     * WOLFSSL_STM32_BARE path; BUILD=cubemx uses ST's HAL drivers via
     * WOLFSSL_STM32_CUBEMX. The per-board family / STM32_RNG / STM32_HASH
     * etc. enables stay identical -- only the dispatch in
     * wolfcrypt/src/port/st/stm32.c differs. */
    #if defined(STM32_BUILD_CUBEMX) && !defined(STM32_HAL_NEWGEN)
        #define WOLFSSL_STM32_CUBEMX
        /* wolfssl's settings.h pulls the HAL umbrella header for each
         * family but is missing WOLFSSL_STM32U0 / WOLFSSL_STM32C0 /
         * WOLFSSL_STM32F3. Include the umbrella here so the
         * CRYP_HandleTypeDef prototype in wolfcrypt/port/st/stm32.h
         * resolves. */
        #if defined(STM32_BOARD_U083)
            #include "stm32u0xx_hal.h"
        #elif defined(STM32_BOARD_C031)
            #include "stm32c0xx_hal.h"
        #elif defined(STM32_BOARD_F303)
            #include "stm32f3xx_hal.h"
        #endif
        /* Tell wolfssl's stm32.c port to auto-enable CRYP / HASH / RNG /
         * PKA peripheral clocks from its wc_Stm32_*_Init() helpers via
         * __HAL_RCC_*_CLK_ENABLE(). Without this define the caller is
         * supposed to enable them in MX_*_Init MSPInit hooks, but the
         * STM32_Bare_Test hw_init_cubemx.c does not generate those --
         * it inits clock + UART directly, then lets wolfcrypt drive
         * everything else lazily. */
        #define STM32_HW_CLOCK_AUTO
        /* On U5 / U3 / L4 / L5 / G0 / G4 the AES IP is TinyAES, exposed
         * as the "AES" peripheral by the HAL (e.g. __HAL_RCC_AES_CLK_ENABLE).
         * wolfssl's stm32.h aliases the CRYP clock macros to AES only for
         * H5 / C5; the other TinyAES families need the same alias on the
         * application side when STM32_HW_CLOCK_AUTO is in effect. */
        #if defined(STM32_BOARD_U585) || defined(STM32_BOARD_U5) || \
            defined(STM32_BOARD_U545) || defined(STM32_BOARD_U3) || \
            defined(STM32_BOARD_L4A6) || defined(STM32_BOARD_L552) || \
            defined(STM32_BOARD_L562) || defined(STM32_BOARD_G071) || \
            defined(STM32_BOARD_G491) || defined(STM32_BOARD_G474) || \
            defined(STM32_BOARD_WBA52) || \
            defined(STM32_BOARD_WL55) || defined(STM32_BOARD_U083)
            #define __HAL_RCC_CRYP_CLK_ENABLE   __HAL_RCC_AES_CLK_ENABLE
            #define __HAL_RCC_CRYP_CLK_DISABLE  __HAL_RCC_AES_CLK_DISABLE
        #elif defined(STM32_BOARD_WB55)
            /* WB has TinyAES exposed as AES1 (not "AES"). */
            #define __HAL_RCC_CRYP_CLK_ENABLE   __HAL_RCC_AES1_CLK_ENABLE
            #define __HAL_RCC_CRYP_CLK_DISABLE  __HAL_RCC_AES1_CLK_DISABLE
        #endif
        /* On U0 / L4 / L5 / G0 / G4 etc. the TinyAES instance is
         * `AES` (not `CRYP`). wolfssl's stm32.h aliases CRYP -> AES
         * for these families inside the WOLFSSL_STM32_BARE block but
         * NOT inside the CUBEMX block, so port code that references
         * `CRYP` directly hits an undeclared identifier. Mirror the
         * alias here under cubemx. */
        #if defined(STM32_BOARD_U083) && !defined(CRYP)
            #define CRYP AES
        #endif

        /* H5 HASH IP renames the instance digest registers from HR[5]
         * to HRA[5]. wolfssl's stm32.h auto-defines
         * WC_STM32_HASH_INSTANCE_HRA only inside the WOLFSSL_STM32_BARE
         * block. Apply the same gate for CUBEMX so stm32.c's HASH
         * reader compiles against HASH->HRA on H5 silicon. */
        #if defined(STM32_BOARD_H5) || defined(STM32_BOARD_H573)
            #define WC_STM32_HASH_INSTANCE_HRA
        #endif

        /* On WBA52 / H7S3 (and likely C5A3) the RNG IP cells need a
         * non-standard CONDRST / NIST-conditioning init that HAL_RNG_Init
         * does not perform -- HAL_RNG_GenerateRandomNumber then times
         * out and wolfssl's wc_GenerateSeed returns RAN_BLOCK_E. The
         * BARE wc_GenerateSeed in wolfcrypt/src/random.c has the right
         * register sequence (see C5 NIST init block) but the CUBEMX
         * path goes through HAL. Until that's wired, drop HW RNG on
         * cubemx for these chips so the bench reaches the algorithm
         * sweep. Wolfssl falls back to its SHA-256 software DRBG. */
        #if defined(STM32_BOARD_WBA52) || defined(STM32_BOARD_H7S3) || \
            defined(STM32_BOARD_C5A3) || defined(STM32_BOARD_C562)
            #undef  STM32_RNG
            #define NO_STM32_RNG
        #endif
        #define BUILD_CONFIG_NAME "bare(cubemx)"
    #else
        /* Direct-register crypto. Reached by BUILD=bare, and also by
         * BUILD=cubemx on a new-generation-HAL part (STM32_HAL_NEWGEN, e.g.
         * STM32C5): that HAL has no classic CRYP/PKA/CCB/RNG driver APIs, so
         * wolfcrypt drives the IP blocks via registers while the new-gen HAL
         * handles only board bring-up (clock/UART). The crypto is identical to
         * a pure BARE build. */
        #define WOLFSSL_STM32_BARE
        /* WOLFSSL_STM32_RNG_NOLIB is auto-defined by settings.h when BARE is set */
        #if defined(STM32_HAL_NEWGEN)
            #define BUILD_CONFIG_NAME "bare(hal-newgen)"
        #else
            #define BUILD_CONFIG_NAME "bare"
        #endif
    #endif
#elif defined(BUILD_ASM)
    /* Cortex-M Thumb2 inline-assembly accelerated software (no STM32 HW).
     * Routes SHA/Chacha/Poly/AES/Curve25519/SHA512 through the
     * wolfcrypt/src/port/arm/thumb2-*-asm_c.c sources, plus SP-math
     * Cortex-M ASM for RSA/DH/ECC. Adds Dilithium (post-quantum sig)
     * with the small-stack variant. */
    #undef  STM32_CRYPTO
    #undef  STM32_HASH
    #undef  STM32_RNG
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define NO_STM32_RNG
    #define WOLFSSL_ARMASM
    #define WOLFSSL_ARMASM_THUMB2
    #define WOLFSSL_ARMASM_INLINE
    #define WOLFSSL_ARMASM_NO_HW_CRYPTO
    #define WOLFSSL_ARMASM_NO_NEON
    #define WOLFSSL_ARM_ARCH 7
    #define WOLFSSL_NO_HASH_RAW
    #define WOLFSSL_ARMASM_AES_BLOCK_INLINE
    #define WC_OMIT_FRAME_POINTER

    /* SP-math Cortex-M ASM for RSA/DH/ECC -- routes ECC P-256 / P-384 /
     * P-521 and the RSA/DH limb math through wolfcrypt/src/sp_cortexm.c.
     * Validated 2026-05-22 across M3 (f207), M4F (g491), M33F (wba52),
     * M7F (h7): full ECC + Curve25519 + Ed25519 PASS.
     *
     * Note: sp_cortexm.c relies on the `register sp_digit* x __asm__("rN")`
     * binding form for AAPCS-fixed arg slots (the inline asm intentionally
     * clobbers the slot of `m` as a scratch in mont_sub etc.). Defining
     * WOLFSSL_NO_VAR_ASSIGN_REG -- which the BUILD_ASM block did pre-cleanup
     * -- removes those bindings and reintroduces the prior UNALIGNED fault
     * on first ECC op. Do NOT add WOLFSSL_NO_VAR_ASSIGN_REG back here. */
    #define WOLFSSL_SP_ARM_CORTEX_M_ASM
    #define WOLFSSL_HAVE_SP_RSA
    #define WOLFSSL_HAVE_SP_DH
    #define WOLFSSL_HAVE_SP_ECC
    #define WOLFSSL_SP_SMALL
    #define SP_WORD_SIZE 32
    #define WOLFSSL_SP_4096
    #define WOLFSSL_SP_384
    #define WOLFSSL_SP_521

    /* Balanced GCM table size (4-bit window) -- not strictly tied to
     * ARMASM but the asm config is typically size-conscious anyway. */
    #define GCM_TABLE_4BIT

    /* Dilithium post-quantum signature (small-stack variant).
     * Dilithium uses SHAKE-128/256 internally; enable SHA-3 + SHAKE.
     * The thumb2-sha3-asm_c.c port exists upstream but pulling it in
     * unconditionally pushes wl55 (256 KB flash) over capacity. SHA3
     * stays on the C path here -- WC_SHA3_NO_ASM scopes the #undef of
     * WOLFSSL_ARMASM to sha3.c only (everywhere else, ARMASM stays
     * active for SHA-256, AES, ChaCha, Curve25519, SHA-512). */
    #define HAVE_DILITHIUM
    #define WOLFSSL_DILITHIUM_SMALL
    #define WOLFSSL_SHA3
    #define WOLFSSL_SHAKE128
    #define WOLFSSL_SHAKE256
    #ifndef STM32_BARE_SHA3_ASM
        #define WC_SHA3_NO_ASM
    #endif

    #define BUILD_CONFIG_NAME "asm"
#elif defined(BUILD_C)
    /* Pure software baseline -- disable all STM32 HW paths */
    #undef  STM32_CRYPTO
    #undef  STM32_HASH
    #undef  STM32_RNG
    #define NO_STM32_CRYPTO
    #define NO_STM32_HASH
    #define NO_STM32_RNG
    #define BUILD_CONFIG_NAME "c"
#else
    #error "Define BUILD_BARE / BUILD_ASM / BUILD_C"
#endif

/* ---------------------------------------------------------------------- */
/* PQC axis (Makefile PQC=1 -> -DSTM32_BARE_PQC)                          */
/* Enables ML-DSA (Dilithium) and ML-KEM with the small-memory variants,  */
/* mirroring the wolfBoot resource-constrained preset. Dilithium needs    */
/* SHA-3 / SHAKE-128 / SHAKE-256, so those are pulled in too. Substantial */
/* code: small-flash boards (c031, g071, u083) may overflow.              */
/* ---------------------------------------------------------------------- */
#if defined(STM32_BARE_PQC)
    /* ML-DSA (Dilithium) - small-memory variants */
    #define HAVE_DILITHIUM
    #define WOLFSSL_DILITHIUM_NO_LARGE_CODE
    #define WOLFSSL_DILITHIUM_SMALL
    #define WOLFSSL_DILITHIUM_VERIFY_SMALL_MEM
    #define WOLFSSL_DILITHIUM_VERIFY_NO_MALLOC

    /* ML-KEM (Kyber) - small-memory variants */
    #define WOLFSSL_HAVE_MLKEM
    #define WOLFSSL_MLKEM_MAKEKEY_SMALL_MEM
    #define WOLFSSL_MLKEM_ENCAPSULATE_SMALL_MEM

    /* Dilithium SHAKE dependencies */
    #ifndef WOLFSSL_SHA3
        #define WOLFSSL_SHA3
    #endif
    #ifndef WOLFSSL_SHAKE128
        #define WOLFSSL_SHAKE128
    #endif
    #ifndef WOLFSSL_SHAKE256
        #define WOLFSSL_SHAKE256
    #endif
    /* asm path scopes WOLFSSL_ARMASM off for sha3.c (thumb2-sha3 ASM
     * is not pulled in -- see wl55 flash-overflow note in Makefile). */
    #if defined(BUILD_ASM) && !defined(STM32_BARE_SHA3_ASM) && \
        !defined(WC_SHA3_NO_ASM)
        #define WC_SHA3_NO_ASM
    #endif
#endif

/* ---------------------------------------------------------------------- */
/* STACK measurement axis (Makefile STACK=1 -> -DSTM32_BARE_STACK_TRACK)  */
/* Enables per-algorithm peak stack + peak heap reporting in wolfcrypt    */
/* benchmark output. Routes XMALLOC through wolfssl's trackable wrapper   */
/* (USE_WOLFSSL_MEMORY) and paints the linker-script stack region with a  */
/* sentinel so HAVE_STACK_SIZE_VERBOSE can walk it for high-water marks.  */
/* The board-side adapter lives in boards/common/board_common.c and is   */
/* invoked from main_bench.c after board_init() returns.                  */
/* ---------------------------------------------------------------------- */
#if defined(STM32_BARE_STACK_TRACK)
    #define USE_WOLFSSL_MEMORY
    #define WOLFSSL_TRACK_MEMORY
    #define WOLFSSL_TRACK_MEMORY_VERBOSE
    #define HAVE_STACK_SIZE
    #define HAVE_STACK_SIZE_VERBOSE
#endif

/* ---------------------------------------------------------------------- */
/* Common settings                                                        */
/* ---------------------------------------------------------------------- */
#define WC_ASYNC_DEV_SIZE (320+24)

/* Single-threaded bare-metal, no filesystem */
#define SINGLE_THREADED
#define NO_FILESYSTEM
#define NO_WRITEV
#define NO_WOLFSSL_SMALL_STACK
#define WOLFSSL_USER_IO
#define NO_MAIN_DRIVER
/* WOLFCRYPT_ONLY drops the TLS layer; the mtls target is the one build that
 * needs it. */
#ifndef STM32_BARE_MTLS
    #define WOLFCRYPT_ONLY
#endif

/* Timing resistance */
#define ECC_TIMING_RESISTANT

/* Test settings */
#define BENCH_EMBEDDED
#define NO_MULTIBYTE_PRINT
#define WOLFSSL_IGNORE_FILE_WARN
#define SIZEOF_LONG_LONG 8
#define NO_DEV_RANDOM
#define WOLFSSL_GENSEED_FORTEST

/* AES - GCM and CCM modes */
#define HAVE_AESGCM
#define HAVE_AESCCM
/* GCM multiply implementation. Default is the 4-bit table (GCM_TABLE_4BIT),
 * which multiplies via gcm->M0; GCM_SMALL multiplies via gcm->H directly.
 * Makefile GCM=small (-> STM32_BARE_GCM_SMALL) selects GCM_SMALL. The DHUK
 * GMAC cross-config KAT builds both and asserts identical tags on the same
 * device key, pinning the GenerateM0() call in Stm32Dhuk_Gmac (a missing M0
 * table only corrupts the table build, not the small build). */
#if defined(STM32_BARE_GCM_SMALL)
    #undef  GCM_TABLE_4BIT
    #define GCM_SMALL
#elif defined(STM32_BARE_GCM_TABLE)
    #undef  GCM_TABLE_4BIT
    #define GCM_TABLE
#else
    #define GCM_TABLE_4BIT
#endif
#define WOLFSSL_AES_DIRECT
#define HAVE_AES_ECB
#define HAVE_AES_DECRYPT
#if defined(STM32_CRYPTO) && !defined(STM32_BOARD_WL55)
    /* Exercise the HW AES-CTR path (the bare wc_AesCtrEncryptBlock transform)
     * in wolfcrypt_test on AES-capable boards. Off for RNG-only parts and for
     * wl55 (256 KB soft-float M4 -- adding CTR overflows flash by ~212 B; the
     * other TinyAES/CRYP/SAES boards still cover this path). */
    #define WOLFSSL_AES_COUNTER
#endif

/* Hashes */
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_HASHDRBG
#define NO_MD4

/* HMAC / KDF */
#define HAVE_HKDF

/* SRAM PUF regression (TARGET=puf). Synthetic-SRAM test mode so it
 * runs on any board without a board-specific NOLOAD section. HKDF is
 * already enabled above. WC_PUF_BCH_T / WC_PUF_NUM_CODEWORDS come from
 * the build command (PUF_T / PUF_CW). */
#ifdef STM32_BARE_PUF
    #define WOLFSSL_PUF
    #define WOLFSSL_PUF_TEST
#endif

/* ChaCha20-Poly1305 */
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ONE_TIME_AUTH

/* ECC */
#define HAVE_ECC
#define WOLFSSL_SP_MATH
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_384
#define WOLFSSL_SP_521
#define WOLFSSL_SP_SMALL
#define SP_WORD_SIZE 32

/* Curve25519 / Ed25519 */
#define HAVE_CURVE25519
#define HAVE_ED25519

/* Cert buffers for test */
#define USE_CERT_BUFFERS_256
#define WOLFSSL_CERT_GEN
#define WOLFSSL_KEY_GEN

/* Drop unused algorithms */
#define NO_RSA
#define NO_DH
#define NO_DSA
#define NO_RC4
#define NO_PKCS12
#define NO_DES3
#define NO_DES3_TLS_SUITES

/* Time -- no RTC, bypass certificate date checking */
#define NO_ASN_TIME
#define WOLFSSL_USER_CURRTIME

/* Diagnostic: uncomment to print which BARE GCM path (HW vs SW fallback)
 * is taken on each call. Off by default to keep benchmark output clean. */
/* #define DEBUG_STM32_BARE_GCM */

/* Error strings */
#define HAVE_ERRNO
#define WOLFSSL_GMTIME

/* ---------------------------------------------------------------------- */
/* U083 size-trim overrides                                               */
/* ---------------------------------------------------------------------- */
/* Default config is ~290 KB of text; U083 has 256 KB flash. Strip the
 * heavy-tail algorithms (P-384/P-521, Curve25519/Ed25519, SHA-384/512,
 * AES-CCM, cert/key gen) to fit. Keep AES + SHA-256 + P-256 + ChaCha20-
 * Poly1305 + HKDF as the minimal viable TLS surface. */
#ifdef STM32_U083_TRIM
    #undef  WOLFSSL_SHA384
    #undef  WOLFSSL_SHA512
    #define NO_SHA384
    #define NO_SHA512
    /* Drop the large SP-math curves -- keep only P-256. ECC P-256
     * still runs in software (no PKA on U0). At 16 MHz on M0+ this
     * is slow -- the full wolfcrypt_test ECC sweep takes several
     * minutes -- but it does PASS. Skipping ECC would lose the
     * coverage; keep it on. */
    #undef  WOLFSSL_SP_384
    #undef  WOLFSSL_SP_521
    /* Drop Curve25519 / Ed25519. */
    #undef  HAVE_CURVE25519
    #undef  HAVE_ED25519
    /* Drop AES-CCM (keep AES-GCM). */
    #undef  HAVE_AESCCM
    /* No cert/key generation. */
    #undef  WOLFSSL_CERT_GEN
    #undef  WOLFSSL_KEY_GEN
#endif

/* C031C6: 32 KB flash / 12 KB RAM. wolfcrypt_test won't fit at all
 * (text alone is ~80+ KB even with all trims). Disable the full suite
 * by undefining the Makefile-injected RUN_WOLFCRYPT_TEST_SUITE flag,
 * and apply the heaviest trim of any board: drop ECC/RSA/DH/RNG/AESGCM,
 * keep only SHA-256 + AES-CBC + ChaCha20-Poly1305 + HKDF as a baseline.
 * main_test.c's KAT block still exercises the path on this board. */
#ifdef STM32_C031_TRIM
    #undef  RUN_WOLFCRYPT_TEST_SUITE
    #undef  WOLFSSL_SHA384
    #undef  WOLFSSL_SHA512
    #define NO_SHA384
    #define NO_SHA512
    #undef  WOLFSSL_SP_384
    #undef  WOLFSSL_SP_521
    #undef  HAVE_CURVE25519
    #undef  HAVE_ED25519
    #undef  HAVE_AESCCM
    #undef  HAVE_AESGCM
    #undef  WOLFSSL_CERT_GEN
    #undef  WOLFSSL_KEY_GEN
    #undef  HAVE_ECC
    #define NO_ECC256
    #undef  HAVE_ECC_KEY_EXPORT
    #undef  HAVE_ECC_KEY_IMPORT
    #undef  HAVE_ECC_DHE
    #undef  HAVE_ECC_SIGN
    #undef  HAVE_ECC_VERIFY
    #define NO_RSA
    #define NO_DH
    #undef  WOLFSSL_SP_4096
    #undef  WOLFSSL_SP_3072
    #undef  WOLFSSL_SP_2048
    #undef  WOLFSSL_SP_1024
    #define NO_ASN
    /* Skip wolfcrypt_test entirely -- doesn't fit. main_test.c KATs
     * (SHA-256 + AES-CBC + AES-ECB + DRBG smoke) are the validation
     * surface on C031. */
    #define WC_NO_HARDEN  /* drop side-channel hardening to save code */
    #define NO_ERROR_STRINGS  /* error.c strings table is ~20 KB */
    #define GCM_SMALL  /* drop GCM tables if AES-GCM crept in via inc */
    #define WOLFSSL_AES_SMALL_TABLES
    #define WOLFSSL_AES_DIRECT
    #define NO_AES_DECRYPT_TABLES
    #define WOLFSSL_NO_MD5
    #define NO_MD5
    #define NO_SHA  /* SHA-1 not needed -- only SHA-256 in KAT */
    #define NO_HMAC  /* main_test.c KAT doesn't use HMAC */
    #define NO_PWDBASED
    #define NO_CHACHAPOLY_AEAD_IUF  /* drop ChaCha incremental API */
    /* C031 has too little flash for ChaCha+Poly+ChaPoly even after
     * other trims. main_test.c KAT block tests SHA-256 + AES only. */
    #define NO_CHACHA_AEAD
    #undef  HAVE_CHACHA
    #undef  HAVE_POLY1305
    #define NO_HASH_WRAPPER  /* drop hash.c generic wrapper */
    /* AES is ~10 KB even small-table -- doesn't fit. C031 baseline
     * becomes "SHA-256 + DRBG smoke" only. The main_test.c AES KAT
     * blocks are #ifdef'd under !NO_AES so they vanish cleanly. */
    #define NO_AES
#endif

/* Callback-only preset (Makefile TARGET=cbonly -> -DSTM32_BARE_CB_ONLY).
 * The four primitives main_cbonly.c exercises -- ECDSA (PKA), AES-GCM (SAES via
 * the DHUK crypto-callback), HMAC-SHA256 (HASH block) and TRNG (RNG) -- all run
 * on hardware, so the software implementations of everything a callback-only
 * build does not need are stripped to shrink flash. Kept: ECC P-256 (+ ASN for
 * DER signatures), AES / AES-GCM, SHA-256, HMAC, RNG. Stripped: RSA, DH, the
 * large SP moduli, SHA-384/512, Curve25519/Ed25519, ChaCha/Poly1305, AES-CCM,
 * PBKDF, cert/key generation and the error-string table. SHA-256 stays because
 * the software Hash-DRBG (used to seed ECC keygen) depends on it. */
#ifdef STM32_BARE_CB_ONLY
    /* True callback-only: strip the software AES and ECC implementations so
     * every AES/ECC op must route through the crypto callback (no SW fallback).
     * AES relies on the wolfSSL aes.c guard that lets the STM32 bare AES path
     * defer to WOLF_CRYPTO_CB_ONLY_AES; ECC relies on the DHUK callback's ECDSA
     * sign + verify handlers (verify -> HW PKA). main_cbonly.c must use a fixed
     * key (no wc_ecc_make_key) since keygen has no device path here. Note: on a
     * sign-only-PKA part (c5a3, WC_STM32_PKA_SIGN_ONLY) there is no HW verify,
     * so WOLF_CRYPTO_CB_ONLY_ECC would break verify there -- keep it to full-PKA
     * boards (u3, u585, u545). */
    #define WOLF_CRYPTO_CB_ONLY_AES
    /* Callback-only ECC only on full-PKA parts. The C5 PKA is sign-only
     * (WC_STM32_PKA_SIGN_ONLY) with no HW ECDSA verify, so stripping software
     * ECC there would leave verify with no implementation. */
    #ifndef WOLFSSL_STM32C5
        #define WOLF_CRYPTO_CB_ONLY_ECC
    #endif
    #define NO_RSA
    #define NO_DH
    #undef  WOLFSSL_SP_4096
    #undef  WOLFSSL_SP_3072
    #undef  WOLFSSL_SP_2048
    #undef  WOLFSSL_SP_1024
    #undef  WOLFSSL_SHA384
    #undef  WOLFSSL_SHA512
    #define NO_SHA384
    #define NO_SHA512
    #undef  WOLFSSL_SP_384
    #undef  WOLFSSL_SP_521
    #undef  HAVE_CURVE25519
    #undef  HAVE_ED25519
    #undef  HAVE_CHACHA
    #undef  HAVE_POLY1305
    #define NO_CHACHA_AEAD
    #define NO_CHACHAPOLY_AEAD_IUF
    #undef  HAVE_AESCCM
    #define NO_PWDBASED
    #undef  WOLFSSL_CERT_GEN
    #undef  WOLFSSL_KEY_GEN
    #define NO_ERROR_STRINGS  /* error.c strings table is ~20 KB */
#endif

/* CubeMX AES crypto-callback test (Makefile TARGET=cubeaes ->
 * -DSTM32_CUBE_AES_ONLY). Turns on WOLF_CRYPTO_CB_ONLY_AES so the software AES
 * core is stripped and every AES op routes through the crypto callback -- the
 * new CubeMX AES device (wc_Stm32_AesRegister). main_cubeaes.c registers it
 * and runs AES-GCM KATs on the HAL. Only AES is exercised; the rest of the
 * common config (ECC/SHA/etc.) stays as software but is unused here. */
#ifdef STM32_CUBE_AES_ONLY
    #define WOLF_CRYPTO_CB_ONLY_AES
#endif

/* CubeMX full HW-crypto callback test (Makefile TARGET=cubecrypto ->
 * -DSTM32_CUBE_CRYPTO_ONLY). Strips software ECC and AES so both route through
 * the crypto callback -- the CubeMX device's HW ECDSA sign/verify (PKA), CCB
 * ECDSA, and HAL AES. This is the customer's config shape; main_cubecrypto.c
 * brings up the ST HAL PKA (hpka + HAL_PKA_Init) that the PKA path needs. */
#ifdef STM32_CUBE_CRYPTO_ONLY
    /* Callback-only ECC only on full-PKA parts, matching STM32_BARE_CB_ONLY
     * above. The C5 PKA is sign-only (WC_STM32_PKA_SIGN_ONLY) with no HW ECDSA
     * verify, so stripping software ECC there would leave verify with no
     * implementation. Latent today (the Makefile pins cubecrypto to BOARD=u3)
     * but keeps the two presets consistent as boards are added. */
    #ifndef WOLFSSL_STM32C5
        #define WOLF_CRYPTO_CB_ONLY_ECC
    #endif
    #define WOLF_CRYPTO_CB_ONLY_AES
#endif

/* G071RB is even tighter -- 128 KB flash / 36 KB RAM, AND no HW crypto
 * at all. The U083 trim still leaves the ECC P-256 SW path in which
 * overflows by ~48 KB. Take a more aggressive cut: drop ECC entirely
 * (no PKA on G071, all ECC is software anyway), drop AES-GCM (keep
 * AES-CBC/CTR), drop RSA and DH. The remaining surface is the symmetric
 * crypto + hashing minimum used by typical embedded TLS-PSK / secure
 * storage workloads. */
#ifdef STM32_G071_TRIM
    #undef  WOLFSSL_SHA384
    #undef  WOLFSSL_SHA512
    #define NO_SHA384
    #define NO_SHA512
    #undef  WOLFSSL_SP_384
    #undef  WOLFSSL_SP_521
    #undef  HAVE_CURVE25519
    #undef  HAVE_ED25519
    #undef  HAVE_AESCCM
    #undef  HAVE_AESGCM
    #undef  WOLFSSL_CERT_GEN
    #undef  WOLFSSL_KEY_GEN
    /* Drop ECC entirely (no HW PKA; SW ECC overflows the 128 KB flash) */
    #undef  HAVE_ECC
    #define NO_ECC256
    #undef  HAVE_ECC_KEY_EXPORT
    #undef  HAVE_ECC_KEY_IMPORT
    #undef  HAVE_ECC_DHE
    #undef  HAVE_ECC_SIGN
    #undef  HAVE_ECC_VERIFY
    /* Drop RSA + DH (asymmetric heavy weight) */
    #define NO_RSA
    #define NO_DH
    /* Drop large math */
    #undef  WOLFSSL_SP_4096
    #undef  WOLFSSL_SP_3072
    #undef  WOLFSSL_SP_2048
    #undef  WOLFSSL_SP_1024
    /* Drop ASN.1 (no certs needed without ECC/RSA) */
    #define NO_ASN
#endif

/* ------------------------------------------------------------------ */
/* ML-DSA-only PQC axis (Makefile PQC=mldsa* -> -DSTM32_BARE_PQC_MLDSA) */
/* ML-DSA only, no ML-KEM, which is what fits the 128 KB boards that   */
/* PQC=1 overflows. The axis splits by which memory variant is used:   */
/*   PQC=mldsa           key gen + sign + verify, small memory.        */
/*   PQC=mldsa-smallest  as above, smallest-memory signer.             */
/*   PQC=mldsa-vfy       verify only, smallest memory, allocated.      */
/*   PQC=mldsa-vfy-pin   verify only, smallest memory, pinned in key.  */
/*   PQC=mldsa-vfy-fast  verify only, small memory, allocated.         */
/* MLDSA_SET=44|65|87 compiles that set alone.                         */
/* Keys are generated in-image, so no ASN.1 is required -- the small-  */
/* flash board trims define NO_ASN.                                   */
/* ------------------------------------------------------------------ */
#if defined(STM32_BARE_PQC_MLDSA)
    #define WOLFSSL_HAVE_MLDSA
    #define WOLFSSL_MLDSA_SMALL
    #define WOLFSSL_MLDSA_NO_LARGE_CODE
    #ifdef STM32_BARE_PQC_MLDSA_VFY
        #define WOLFSSL_MLDSA_VERIFY_ONLY
        /* Smallest verify RAM: vector z is streamed a polynomial at a time.
         * STM32_BARE_PQC_MLDSA_FAST trades that back for speed. */
        #ifndef STM32_BARE_PQC_MLDSA_FAST
            #define WOLFSSL_MLDSA_VERIFY_SMALLEST_MEM
        #endif
        /* Pin the verify buffers in the key instead of allocating them, for
         * a build with no allocator at all. Costs ~5 KB of key object. */
        #ifdef STM32_BARE_PQC_MLDSA_VFY_PIN
            #define WOLFSSL_MLDSA_VERIFY_NO_MALLOC
        #endif
    #else
        #define WOLFSSL_MLDSA_MAKE_KEY_SMALL_MEM
        #ifdef STM32_BARE_PQC_MLDSA_SIGN_SMALLEST
            #define WOLFSSL_MLDSA_SIGN_SMALLEST_MEM
        #else
            #define WOLFSSL_MLDSA_SIGN_SMALL_MEM
        #endif
        #define WOLFSSL_MLDSA_DYNAMIC_KEYS
    #endif
    #define WOLFSSL_MLDSA_VERIFY_SMALL_MEM
    #define WOLFSSL_MLDSA_NO_ASN1
    /* ARMv6-M cannot do unaligned word access. */
    #define WOLFSSL_MLDSA_ALIGNMENT 4
    #ifdef STM32_BARE_PQC_MLDSA_SET
        #if STM32_BARE_PQC_MLDSA_SET == 44
            #define WOLFSSL_NO_ML_DSA_65
            #define WOLFSSL_NO_ML_DSA_87
        #elif STM32_BARE_PQC_MLDSA_SET == 65
            #define WOLFSSL_NO_ML_DSA_44
            #define WOLFSSL_NO_ML_DSA_87
        #elif STM32_BARE_PQC_MLDSA_SET == 87
            #define WOLFSSL_NO_ML_DSA_44
            #define WOLFSSL_NO_ML_DSA_65
        #else
            #error "STM32_BARE_PQC_MLDSA_SET must be 44, 65 or 87"
        #endif
    #endif
    #ifndef WOLFSSL_SHA3
        #define WOLFSSL_SHA3
    #endif
    #ifndef WOLFSSL_SHAKE128
        #define WOLFSSL_SHAKE128
    #endif
    #ifndef WOLFSSL_SHAKE256
        #define WOLFSSL_SHAKE256
    #endif
    #if defined(BUILD_ASM) && !defined(STM32_BARE_SHA3_ASM) && \
        !defined(WC_SHA3_NO_ASM)
        #define WC_SHA3_NO_ASM
    #endif
#endif

/* ------------------------------------------------------------------ */
/* mTLS axis (Makefile TARGET=mtls -> -DSTM32_BARE_MTLS)                */
/* TLS 1.3 only, ECDHE-ECDSA P-256, AES-GCM. The base config above      */
/* already supplies AES-GCM, SHA-256/384, HKDF and ECC.                 */
/* ------------------------------------------------------------------ */
#ifdef STM32_BARE_MTLS
    #define WOLFSSL_TLS13
    #define WOLFSSL_NO_TLS12
    #define NO_OLD_TLS
    #define HAVE_TLS_EXTENSIONS
    #define HAVE_SUPPORTED_CURVES
    #define HAVE_EXTENDED_MASTER
    #define NO_SESSION_CACHE
    #define WOLFSSL_NO_SOCK
    /* Client and server both run in this image, over an in-memory
     * transport -- no sockets, no network stack. */
    #define USE_CERT_BUFFERS_256
#endif

#ifdef __cplusplus
}
#endif

#endif /* WOLF_USER_SETTINGS_H */
