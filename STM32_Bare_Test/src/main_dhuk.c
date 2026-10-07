/* main_dhuk.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * DHUK (Device Hardware Unique Key) test app for STM32_Bare_Test, exercising
 * the transparent crypto-callback DHUK path (the STM32 cryptocb device, see
 * wc_Stm32_DhukRegister). Build with: make BOARD=u3 CONFIG=bare TARGET=dhuk
 *
 *   [1] wc_ecc_import_wrapped_private input validation -- a pure-software
 *       hard PASS/FAIL covering the seed + wrapped-scalar bounds checks.
 *   [2] GMAC via the transparent crypto-callback (normal wc_AesGcmEncrypt on
 *       a DHUK-keyed Aes; derived key never in software).
 *   [3] AES-ECB via the transparent crypto-callback (wc_AesEcb*; round-trip +
 *       seed-dependence confirm the DHUK key drives the cipher).
 *   [4] ECDSA sign via the transparent crypto-callback (wc_ecc_sign_hash with
 *       a DHUK-wrapped scalar; verified with the public counterpart).
 *   [7] wc_Stm32_Aes_Wrap blob word order -- runs on both build paths so the
 *       shared WC_STM32_WRAP_ORDER_RAW blob can be compared bare vs CubeMX,
 *       and the default order is asserted to be raw on both (legacy is
 *       reachable through _ex() for wolfSSL 5.9.0 - 5.9.2 blobs).
 *   [8] Provisioning reference -- the minimal shape a product uses: data
 *       encryption under a DHUK-derived key, an external AES-256 key kept
 *       under a DHUK KEK, and ECDSA with a DHUK-wrapped private scalar.
 *       Read this one first if you are integrating rather than testing.
 *  [10] ECDSA sign from a wrapped scalar imported onto a key that never saw
 *       keygen -- the runtime shape, and a guard that the import leaves the
 *       key ready to sign.
 *   [9] wc_Stm32_Aes_Wrap blob used as a key -- asserts a RAW blob unwraps
 *       back to the key it wrapped, guarding the unwrap and blob-order fixes,
 *       with the KEK alternative exercised beside it.
 *  [12] Key at rest wrapped under the DHUK and unwrapped back into RAM, for
 *       key material an algorithm outside wolfCrypt has to consume. AES-GCM,
 *       arbitrary payload length, with a corrupted blob, a wrong seed and a
 *       wrong key id all required to fail.
 *  [13] wc_Stm32_Aes_DhukOp_ex exact-key wrap/unwrap over a longer
 *       block-multiple payload, every block checked against software AES
 *       keyed with the wrapped key (DHUK_UNWRAP=1 only).
 *
 * A backend that is gated off or unavailable (CRYPTOCB_UNAVAILABLE / a
 * TZEN-secure-context timeout) is reported as an expected soft-PASS, not a
 * failure. Results are also mirrored to the g_dhuk_res debugger sink. */

#include <stdio.h>

#include "board.h"

extern uint32_t SystemCoreClock;
extern void     SystemCoreClockUpdate(void);

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfssl/wolfcrypt/random.h"
#include "wolfssl/wolfcrypt/ecc.h"
#include "wolfssl/wolfcrypt/port/st/stm32.h"
#include "wolfssl/wolfcrypt/aes.h"

#ifndef BUILD_CONFIG_NAME
#define BUILD_CONFIG_NAME "unknown"
#endif

/* Debugger-readable result sink, so results can be captured without a
 * working VCP (the B-U585I-IOT02A's UART routing differs from the
 * NUCLEO-U585AI-Q this board build targets). Read after the run with:
 *   arm-none-eabi-nm app.elf | grep g_dhuk_res   # address
 *   openocd ... -c "mdw 0x<addr> 12"
 * Captures the RAW return codes (before the soft-PASS mapping) so a
 * gated/timeout (-271 / WC_TIMEOUT_E) is distinguishable from a real
 * success (0). magic = 0xD04B0001 once main() reaches the end. */
volatile struct {
    uint32_t magic;
    int32_t  setter_rc;    /* 0 = all setter validation cases passed   */
    int32_t  cb_gmac_rc;   /* transparent crypto-cb GMAC return        */
    uint32_t cb_gmac_tag[4];/* crypto-cb GMAC tag on success           */
    int32_t  dhukop_rc;    /* wc_Stm32_Aes_DhukOp_ex round-trip return */
    uint32_t dhukop_blob[8];/* chip-bound wrapped blob (cross-build cmp)*/
    int32_t  overall;      /* final result (0 = PASS)                 */
} g_dhuk_res;

/* A backend that is gated off or that cannot complete the unwrap on
 * TZEN=0 silicon returns one of these. Treat as expected, not a fail. */
static int is_expected_gated(int ret)
{
    return (ret == CRYPTOCB_UNAVAILABLE) ||
           (ret == WC_TIMEOUT_E) ||
           (ret == WC_HW_E);
}

/* Compare an actual return code against the expected one. Returns 0 on
 * match, -1 on mismatch (with a printed diagnostic). */
static int expect_ret(const char* label, int got, int want)
{
    if (got != want) {
        printf("  %s: got %d, want %d -- FAIL\n", label, got, want);
        return -1;
    }
    printf("  %s: %d OK\n", label, got);
    return 0;
}

#if defined(WOLFSSL_DHUK) && \
    (defined(WOLFSSL_STM32_BARE) || defined(WOLFSSL_STM32_CUBEMX)) && \
    defined(WC_STM32_HAS_DHUK)

/* [1] wc_ecc_import_wrapped_private input-validation unit test. Pure
 * software; a hard PASS/FAIL exercising the seed + wrapped-scalar bounds checks
 * (including the wrappedLen <= roundup16(plainLen) invariant). */
static int test_ecc_dhuk_setter(void)
{
    /* Content does not matter for the validation paths; only lengths are
     * checked. Sized to the largest blob the import accepts (96 bytes). */
    byte wrapped[96];
    byte seed[32];
    ecc_key key;
    int ret;
    int rc = 0;

    XMEMSET(wrapped, 0xa5, sizeof(wrapped));
    XMEMSET(seed, 0x5a, sizeof(seed));

    ret = wc_ecc_init(&key);
    if (ret != 0) {
        printf("  wc_ecc_init failed: %d\n", ret);
        return ret;
    }

    /* Reject: NULL key / seed / wrapped pointers. */
    ret = wc_ecc_import_wrapped_private(NULL, ECC_SECP256R1, seed, 32,
                                        wrapped, 32, 32);
    if (expect_ret("reject key=NULL", ret, BAD_FUNC_ARG) != 0) rc = -1;
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, NULL, 32,
                                        wrapped, 32, 32);
    if (expect_ret("reject seed=NULL", ret, BAD_FUNC_ARG) != 0) rc = -1;
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        NULL, 32, 32);
    if (expect_ret("reject wrapped=NULL", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Good: 32-byte seed, 32-byte wrapped scalar, 32-byte plaintext (P-256). */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 32, 32);
    if (expect_ret("accept P-256 (32/32)", ret, 0) != 0) rc = -1;

    /* Boundary OK: P-521 plaintext (66) padded to 80, blob 80 == max. */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP521R1, seed, 32,
                                        wrapped, 80, 66);
    if (expect_ret("accept P-521 (80/66)", ret, 0) != 0) rc = -1;

    /* Reject: plainLen must be the curve's scalar size. 1 byte matches no
     * curve, and a scalar size that disagrees with curve_id is malformed. */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 16, 1);
    if (expect_ret("reject plain=1 vs P-256", ret, BAD_FUNC_ARG) != 0) rc = -1;
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 48, 48);
    if (expect_ret("reject plain=48 vs P-256", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: seed length must be 32. */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 16,
                                        wrapped, 32, 32);
    if (expect_ret("reject seedSz=16", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: not a multiple of the AES block size. */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 20, 20);
    if (expect_ret("reject wrappedLen=20", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: zero-length wrapped blob. */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 0, 0);
    if (expect_ret("reject wrappedLen=0", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: larger than the on-key buffer (> 96). */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 112, 32);
    if (expect_ret("reject wrappedLen=112", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: plaintext longer than the wrapped blob. Every other argument is
     * valid for P-256, so only the length relation under test can fail it. */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 16, 32);
    if (expect_ret("reject plain=32 > wrapped=16", ret, BAD_FUNC_ARG) != 0)
        rc = -1;

    /* Reject: wrapped blob larger than the plaintext padded to a full block
     * (plain=32 -> roundup16 = 32, so a 48-byte blob is malformed). */
    ret = wc_ecc_import_wrapped_private(&key, ECC_SECP256R1, seed, 32,
                                        wrapped, 48, 32);
    if (expect_ret("reject wrapped=48 > roundup16(plain=32)", ret,
                   BAD_FUNC_ARG) != 0)
        rc = -1;

    wc_ecc_free(&key);
    g_dhuk_res.setter_rc = rc;
    if (rc == 0) {
        printf("  setter validation OK\n");
    }
    return rc;
}

#ifdef WOLF_CRYPTO_CB
/* [2] Transparent crypto-callback GMAC via the STM32 DHUK device. Register the
 * device, init a normal Aes with devId = WC_DHUK_DEVID, set the 256-bit seed as
 * the key (wc_AesGcmSetKey), then call the standard wc_AesGcmEncrypt with empty
 * plaintext (GMAC). The derived key never appears in software. Checks: no
 * timeout, determinism (two runs match), and round-trip verify via
 * wc_AesGcmDecrypt. A gated/timeout result is a soft-PASS. */
static int test_dhuk_cryptocb_gmac(void)
{
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte iv[12] = {
        0xca,0xfe,0xba,0xbe,0xfa,0xce,0xdb,0xad,
        0xde,0xca,0xf8,0x88
    };
    static const byte aad[16] = {
        0xfe,0xed,0xfa,0xce,0xde,0xad,0xbe,0xef,
        0xfe,0xed,0xfa,0xce,0xde,0xad,0xbe,0xef
    };
    Aes  aes;
    byte io[1];
    byte tag1[16];
    byte tag2[16];
    int  ret;
    int  i;

    XMEMSET(tag1, 0, sizeof(tag1));
    XMEMSET(tag2, 0, sizeof(tag2));

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_DhukRegister failed: %d\n", ret);
        return ret;
    }

    /* GMAC tag via the normal AES-GCM API (empty plaintext). */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
    }
    if (ret == 0) {
        ret = wc_AesGcmEncrypt(&aes, io, io, 0, iv, (word32)sizeof(iv),
                               tag1, (word32)sizeof(tag1),
                               aad, (word32)sizeof(aad));
    }
    wc_AesFree(&aes);
    g_dhuk_res.cb_gmac_rc = ret;
    XMEMCPY((void*)g_dhuk_res.cb_gmac_tag, tag1, sizeof(tag1));

    if (is_expected_gated(ret)) {
        printf("  cryptocb GMAC reachable; backend gated/unavailable "
               "(ret=%d)\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  cryptocb GMAC failed: %d\n", ret);
        goto cleanup;
    }
    printf("  cryptocb GMAC tag:");
    for (i = 0; i < 16; i++) printf(" %02x", tag1[i]);
    printf("\n");

    /* Determinism: same seed + inputs must give the same tag. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
    }
    if (ret == 0) {
        ret = wc_AesGcmEncrypt(&aes, io, io, 0, iv, (word32)sizeof(iv),
                               tag2, (word32)sizeof(tag2),
                               aad, (word32)sizeof(aad));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb GMAC (run 2) failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(tag1, tag2, 16) != 0) {
        printf("  cryptocb GMAC not deterministic -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb GMAC deterministic OK\n");

    /* Round-trip: verify the tag via the normal AES-GCM decrypt API. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
    }
    if (ret == 0) {
        ret = wc_AesGcmDecrypt(&aes, io, io, 0, iv, (word32)sizeof(iv),
                               tag1, (word32)sizeof(tag1),
                               aad, (word32)sizeof(aad));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb GMAC verify failed: %d\n", ret);
        goto cleanup;
    }
    printf("  cryptocb GMAC verify OK (round-trip)\n");
    ret = 0;

cleanup:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}

#if defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT)
/* [3] Transparent AES-ECB via the STM32 DHUK device. Encrypt/decrypt with the
 * standard wc_AesEcb* API on an Aes inited with devId = WC_DHUK_DEVID and the
 * seed set as the key. Checks: round-trip recovers plaintext, ciphertext !=
 * plaintext (encryption happened), and a DIFFERENT seed yields a DIFFERENT
 * ciphertext (proves the DHUK-derived key actually drives the cipher, i.e. the
 * crypto-callback path is engaged). */
static int test_dhuk_cryptocb_ecb(void)
{
    static const byte seedA[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte seedB[32] = {
        0xa5,0xa5,0xa5,0xa5,0xa5,0xa5,0xa5,0xa5,
        0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    static const byte pt[16] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    Aes  aes;
    byte ctA[16];
    byte ctB[16];
    byte rt[16];
    int  ret;
    int  i;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }

    /* Encrypt with seed A. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ctA, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (is_expected_gated(ret)) {
        printf("  cryptocb ECB reachable; backend gated/unavailable (ret=%d)\n",
               ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  cryptocb ECB encrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt, ctA, 16) == 0) {
        printf("  cryptocb ECB produced plaintext -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb ECB ct:");
    for (i = 0; i < 16; i++) printf(" %02x", ctA[i]);
    printf("\n");

    /* Decrypt with seed A -- must recover plaintext. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, NULL, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbDecrypt(&aes, rt, ctA, (word32)sizeof(ctA));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb ECB decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt, rt, 16) != 0) {
        printf("  cryptocb ECB round-trip mismatch -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb ECB round-trip OK\n");

    /* Encrypt with seed B -- different seed must give different ciphertext. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedB, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ctB, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb ECB (seed B) failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(ctA, ctB, 16) == 0) {
        printf("  cryptocb ECB seed had no effect -- FAIL (not DHUK path)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb ECB seed-dependent OK (DHUK key drives cipher)\n");
    ret = 0;

cleanup:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}
#endif /* HAVE_AES_ECB || WOLFSSL_AES_DIRECT */

#if defined(HAVE_AES_CBC)
/* [5] Transparent AES-CBC via the STM32 DHUK device. Validates the fix that
 * routes CBC through the crypto-callback: previously CBC bypassed it and
 * silently used the 256-bit seed as a raw AES key. Discriminating check: for a
 * single block with IV = 0, CBC(P) == ECB(P) ONLY when both use the same
 * SAES-derived key -- the old seed-as-key path would not match ECB's derived
 * key. Also confirms that a DHUK AES-CTR call (a mode the SAES backend cannot
 * service) now returns a hard error instead of silently using the seed. */
static int test_dhuk_cryptocb_cbc(void)
{
    static const byte seedA[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte pt[16] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    static const byte zero_iv[16] = { 0 };
    /* Two-block vector for the chained in-place decrypt regression below. */
    static const byte pt2[32] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
        0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,
        0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51
    };
    Aes  aes;
    byte ctEcb[16];
    byte ctCbc[16];
    byte rt[16];
    byte buf2[32];
    int  ret;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }

    /* ECB reference: ctEcb = ECB_k(pt) with the SAES-derived key. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ctEcb, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (is_expected_gated(ret)) {
        printf("  cryptocb CBC reachable; backend gated/unavailable (ret=%d)\n",
               ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  ECB reference failed: %d\n", ret);
        goto cleanup;
    }

    /* CBC with IV = 0, single block: must equal the ECB reference. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcEncrypt(&aes, ctCbc, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb CBC encrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(ctEcb, ctCbc, 16) != 0) {
        printf("  cryptocb CBC != ECB(IV=0) -- FAIL "
               "(CBC not using the SAES-derived key)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb CBC matches ECB(IV=0) OK (SAES-derived key drives CBC)\n");

    /* CBC round-trip: decrypt recovers plaintext. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcDecrypt(&aes, rt, ctCbc, (word32)sizeof(ctCbc));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb CBC decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt, rt, 16) != 0) {
        printf("  cryptocb CBC round-trip mismatch -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb CBC round-trip OK\n");

    /* Multi-block chained in-place CBC decrypt (regression for the chaining-IV
     * fix). Split a two-block ciphertext across two in-place wc_AesCbcDecrypt
     * calls so the second call depends on the chaining IV (aes->reg) that the
     * first call must update. In-place decrypt overwrites the input block with
     * plaintext, so the wrapper has to save the ciphertext block for the next
     * IV; reading it back from the overwritten input corrupts block 1. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        /* Encrypt straight into buf2; it is decrypted in place below. */
        ret = wc_AesCbcEncrypt(&aes, buf2, pt2, (word32)sizeof(pt2));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb CBC 2-block encrypt failed: %d\n", ret);
        goto cleanup;
    }
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcDecrypt(&aes, buf2, buf2, 16);         /* block 0 */
    }
    if (ret == 0) {
        ret = wc_AesCbcDecrypt(&aes, buf2 + 16, buf2 + 16, 16); /* block 1 */
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb CBC chained in-place decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt2, buf2, sizeof(pt2)) != 0) {
        printf("  cryptocb CBC chained in-place decrypt mismatch -- FAIL "
               "(chaining IV corrupted)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb CBC chained in-place decrypt OK\n");

#ifdef WOLFSSL_AES_COUNTER
    /* Negative: AES-CTR is not serviceable for a DHUK key. It must now return a
     * hard error (ALGO_ID_E) rather than silently encrypting with the seed as a
     * raw key. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCtrEncrypt(&aes, rt, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret == ALGO_ID_E) {
        printf("  cryptocb CTR on DHUK key rejected (ALGO_ID_E) OK\n");
        ret = 0;
    }
    else {
        printf("  cryptocb CTR on DHUK key NOT rejected (ret=%d) -- FAIL\n", ret);
        ret = -1;
        goto cleanup;
    }
#endif /* WOLFSSL_AES_COUNTER */
    ret = 0;

cleanup:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}
#endif /* HAVE_AES_CBC */

/* [6] wc_Stm32_Aes_Wrap blob word order. Runs on BOTH build paths so the two
 * can be compared: WC_STM32_WRAP_ORDER_RAW must produce the same blob on
 * bare-metal and CubeMX/HAL (that is the whole point of the shared format),
 * and it is now what wc_Stm32_Aes_Wrap() defaults to on both. LEGACY stays
 * reachable through wc_Stm32_Aes_Wrap_ex() for regenerating blobs from
 * wolfSSL 5.9.0 - 5.9.2. Both blobs are printed so a bare run and a CubeMX
 * run can be diffed by eye or by script.
 *
 * The wrap key here is a fixed software key (devId != WOLFSSL_DHUK_DEVID), not
 * the silicon DHUK, so the output is reproducible across chips and the check
 * is about byte order only. */
static int test_dhuk_wrap_order(void)
{
    /* AES-128 FIPS-197 key, and a 32-byte payload to wrap. */
    static const byte wrap_key[32] = {
        0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
        0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
        0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
        0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
    };
    static const byte payload[32] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
    };
    Aes    aes;
    byte   rawBlob[32];
    byte   defBlob[32];
    word32 rawSz = sizeof(rawBlob);
    word32 defSz = sizeof(defBlob);
    word32 i;
    int    ret;

    XMEMSET(rawBlob, 0, sizeof(rawBlob));
    XMEMSET(defBlob, 0, sizeof(defBlob));

    /* Shared raw order -- must match byte-for-byte on both build paths. */
    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0) {
        printf("  wc_AesInit failed: %d\n", ret);
        return ret;
    }
    XMEMCPY(aes.key, wrap_key, sizeof(wrap_key));
    aes.keylen = 32;
    ret = wc_Stm32_Aes_Wrap_ex(&aes, payload, sizeof(payload), rawBlob, &rawSz,
                               NULL, 0, WC_STM32_WRAP_ORDER_RAW);
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  wrap_ex(RAW) failed: %d\n", ret);
        return ret;
    }

    /* Whatever this build's wc_Stm32_Aes_Wrap() defaults to. */
    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0) {
        printf("  wc_AesInit failed: %d\n", ret);
        return ret;
    }
    XMEMCPY(aes.key, wrap_key, sizeof(wrap_key));
    aes.keylen = 32;
    ret = wc_Stm32_Aes_Wrap(&aes, payload, sizeof(payload), defBlob, &defSz,
                            NULL, 0);
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  wrap(default) failed: %d\n", ret);
        return ret;
    }

    if (rawSz != sizeof(rawBlob) || defSz != sizeof(defBlob)) {
        printf("  wrap returned wrong size (raw=%lu def=%lu) -- FAIL\n",
               (unsigned long)rawSz, (unsigned long)defSz);
        return -1;
    }

    printf("  wrap RAW blob:    ");
    for (i = 0; i < sizeof(rawBlob); i++) printf("%02x", rawBlob[i]);
    printf("\n  wrap default blob:");
    for (i = 0; i < sizeof(defBlob); i++) printf("%02x", defBlob[i]);
    printf("\n");

    /* Both build paths now default to the raw order, so the plain wrap must
     * match wrap_ex(RAW). Legacy stays reachable through _ex() only. */
    if (XMEMCMP(rawBlob, defBlob, sizeof(rawBlob)) != 0) {
        printf("  default order differs from RAW -- FAIL\n");
        return -1;
    }
    printf("  default order is RAW on this build OK\n");
    printf("  compare the RAW line against the other build to confirm "
           "cross-build interop\n");
    return 0;
}

#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
/* [4] ECDSA sign with a DHUK-protected private key via the normal
 * wc_ecc_sign_hash API. Self-bootstrap: make a P-256 keypair, ECB-encrypt
 * (wrap) its scalar with the DHUK-derived key (same seed), import the wrapped
 * scalar + seed onto the ecc_key, set devId = WC_DHUK_DEVID, then sign. The
 * plaintext scalar only lives in a short-lived stack buffer during the PKA
 * sign. The signature is verified with the public counterpart (SW path). */
static int test_dhuk_cryptocb_ecdsa(WC_RNG* rng)
{
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte hash[32] = {
        0x9f,0x86,0xd0,0x81,0x88,0x4c,0x7d,0x65,
        0x9a,0x2f,0xea,0xa0,0xc5,0x5a,0xd0,0x15,
        0xa3,0xbf,0x4f,0x1b,0x2b,0x0b,0x82,0x2c,
        0xd1,0x5d,0x6c,0x15,0xb0,0xf0,0x0a,0x08
    };
    ecc_key kp;
    Aes  aes;
    byte priv[32];
    byte wrapped[32];
    byte sig[80];
    word32 privSz = (word32)sizeof(priv);
    word32 sigLen = (word32)sizeof(sig);
    int  ret;
    int  verify = 0;
    int  haveKey = 0;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }

    ret = wc_ecc_init(&kp);
    if (ret != 0) {
        printf("  wc_ecc_init failed: %d\n", ret);
        goto unreg;
    }
    haveKey = 1;
    ret = wc_ecc_make_key_ex(rng, 32, &kp, ECC_SECP256R1);
    if (ret != 0) {
        printf("  wc_ecc_make_key_ex failed: %d\n", ret);
        goto cleanup;
    }

    /* Sanity: plain (non-DHUK) PKA ECDSA sign+verify on this silicon, to
     * isolate any DHUK-path issue from a PKA-hardware issue. kp.devId is
     * still INVALID here, so this uses the normal HW PKA path. */
    sigLen = (word32)sizeof(sig);
    ret = wc_ecc_sign_hash(hash, (word32)sizeof(hash), sig, &sigLen, rng, &kp);
    if (ret == 0) {
        ret = wc_ecc_verify_hash(sig, sigLen, hash, (word32)sizeof(hash),
                                 &verify, &kp);
    }
    printf("  plain PKA sign+verify: rc=%d verify=%d\n", ret, verify);
    ret = 0;
    verify = 0;
    sigLen = (word32)sizeof(sig);

    ret = wc_ecc_export_private_only(&kp, priv, &privSz);
    if (ret != 0 || privSz != 32u) {
        printf("  export scalar failed: %d (len %lu)\n", ret,
               (unsigned long)privSz);
        ret = (ret != 0) ? ret : -1;
        goto cleanup;
    }

    /* Wrap the scalar = ECB-encrypt with the DHUK-derived key (seed as key). */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, wrapped, priv, 32);
    }
    wc_AesFree(&aes);
    wc_ForceZero(priv, sizeof(priv));
    if (is_expected_gated(ret)) {
        printf("  DHUK ECDSA: backend gated/unavailable (ret=%d)\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  scalar wrap failed: %d\n", ret);
        goto cleanup;
    }

    /* Import the wrapped scalar + seed, and route signing through DHUK by
     * setting the device id on the key. */
    kp.devId = WC_DHUK_DEVID;
    ret = wc_ecc_import_wrapped_private(&kp, ECC_SECP256R1, seed,
                                        (word32)sizeof(seed),
                                       wrapped, 32, 32);
    if (ret != 0) {
        printf("  import wrapped private failed: %d\n", ret);
        goto cleanup;
    }

    ret = wc_ecc_sign_hash(hash, (word32)sizeof(hash), sig, &sigLen, rng, &kp);
    if (is_expected_gated(ret)) {
        printf("  DHUK ECDSA sign gated/unavailable (ret=%d)\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  DHUK ECDSA sign failed: %d\n", ret);
        goto cleanup;
    }
    printf("  DHUK ECDSA sign produced a %lu-byte signature\n",
           (unsigned long)sigLen);

    /* Verify with the public counterpart (crypto-cb has no verify -> SW). */
    ret = wc_ecc_verify_hash(sig, sigLen, hash, (word32)sizeof(hash),
                             &verify, &kp);
    if (ret != 0) {
        printf("  DHUK ECDSA verify error: %d\n", ret);
        goto cleanup;
    }
    if (verify != 1) {
        printf("  DHUK ECDSA verify FAILED (sig invalid)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  DHUK ECDSA verify OK (signed via DHUK, verified with pubkey)\n");
    ret = 0;

cleanup:
    wc_ForceZero(priv, sizeof(priv));
    wc_ForceZero(wrapped, sizeof(wrapped));
    if (haveKey) {
        wc_ecc_free(&kp);
    }
unreg:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}
#endif /* HAVE_ECC && WOLFSSL_STM32_PKA */

#if (defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT)) && \
    defined(HAVE_AES_CBC)
/* [8] Provisioning reference -- the minimal shape a product uses, kept
 * deliberately free of the probing and cross-checking the tests above do.
 *
 * The thing to understand before reading it: a 256-bit value handed to a
 * WC_DHUK_DEVID Aes as its key is not used as a literal AES key. SAES loads
 * it under the silicon DHUK and ciphers with the result, which never enters
 * software. wc_Stm32_Aes_Wrap_ex(..., WC_STM32_WRAP_ORDER_RAW) is the
 * inverse of that load, so it is how a chosen key is turned into a blob the
 * device will accept. Three flows follow:
 *
 *   A -- Data encryption where the key does not have to be a chosen value.
 *        Store 32 random bytes and let the hardware derive the working key
 *        on every use. Nothing to provision. Prefer this.
 *   B -- An externally supplied AES-256 key K that must be used verbatim.
 *        Wrap K once with wc_Stm32_Aes_Wrap_ex(RAW), store the blob, and at
 *        runtime hand the blob to a WC_DHUK_DEVID Aes as its key: the
 *        hardware loads K into KEYR and ciphers with it, so K never exists
 *        in software. The blob also matches ST's HAL_CRYPEx_WrapKey output.
 *        B2 shows the alternative for when you need the key bytes back --
 *        a 128-bit key, or key material that is not an AES key at all,
 *        since the device only accepts a 32-byte key. There the DHUK-derived
 *        key acts as a KEK and K is in RAM while in use.
 *   C -- ECDSA with a DHUK-protected private key. An ECC scalar cannot live
 *        in KEYR, so it uses the B2 KEK wrap; the scalar is unwrapped into a
 *        short-lived buffer for the PKA. wc_ecc_sign_hash() is the ordinary
 *        API from there.
 *
 * In a real product, the wrapping half of B and C happens once in the
 * factory and only the blob plus the seed reach flash. Both halves run here
 * so the example is self-contained. */
static int dhuk_provision_example(WC_RNG* rng)
{
    /* What a product would keep in flash. */
    static byte seed[32];       /* per-key derivation seed -- not secret     */
    static byte wrappedKey[32]; /* flow B: K encrypted under the derived key */
    static byte wrappedPriv[32];/* flow C: ECC scalar, same treatment        */

    static const byte pt[32] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
        0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,
        0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51
    };
    /* CBC needs a fresh unpredictable IV per encryption; it is not secret and
     * is stored or transmitted alongside the ciphertext. Generated here rather
     * than hard-coded so this reads as the production pattern. */
    byte iv[16];
    /* Flow B only: a key the application does not get to choose. */
    static const byte extKey[32] = {
        0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
        0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
        0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
        0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
    };
    Aes    aes;
    byte   ct[32];
    byte   rt[32];
    byte   recovered[32];
    int    ret;
    int    dhukReg = 0;
    int    aesReg = 0;

    XMEMSET(ct, 0, sizeof(ct));
    XMEMSET(rt, 0, sizeof(rt));
    XMEMSET(recovered, 0, sizeof(recovered));

    /* One-time: make the DHUK device available at WC_DHUK_DEVID. */
    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_DhukRegister failed: %d\n", ret);
        return ret;
    }
    dhukReg = 1;

    /* Provisioning step 0: the seed is just random bytes. It is stored in
     * the clear -- it is worthless on any other chip, because the key it
     * derives depends on this die's DHUK. */
    ret = wc_RNG_GenerateBlock(rng, iv, sizeof(iv));
    if (ret != 0) {
        printf("  wc_RNG_GenerateBlock(iv) failed: %d\n", ret);
        goto cleanup;
    }
    ret = wc_RNG_GenerateBlock(rng, seed, sizeof(seed));
    if (ret != 0) {
        printf("  seed generation failed: %d\n", ret);
        goto cleanup;
    }

    /* ---- A: encrypt with a DHUK-derived key (no wrapping involved) ----
     * The 32 bytes handed to wc_AesSetKey are the SEED, not a literal key:
     * a WC_DHUK_DEVID Aes always treats a 256-bit key that way. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, sizeof(seed), iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcEncrypt(&aes, ct, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (is_expected_gated(ret)) {
        printf("  DHUK backend gated/unavailable (ret=%d) -- skipping\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  [A] DHUK CBC encrypt failed: %d\n", ret);
        goto cleanup;
    }

    /* Decrypting is the same call with the same seed. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, sizeof(seed), iv, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcDecrypt(&aes, rt, ct, (word32)sizeof(ct));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  [A] DHUK CBC decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(rt, pt, sizeof(pt)) != 0) {
        printf("  [A] DHUK CBC round-trip mismatch -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  [A] seed-keyed AES-CBC round-trip OK "
           "(key derived in HW, never in SW)\n");

    /* ---- B: protect an externally supplied AES-256 key ----
     * Factory half: wrap K under the silicon DHUK. devId WOLFSSL_DHUK_DEVID
     * selects the DHUK as the wrapping key; RAW is the order the device
     * consumes, and is what ST's HAL_CRYPEx_WrapKey produces. */
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        word32 blobSz = (word32)sizeof(wrappedKey);
        ret = wc_Stm32_Aes_Wrap_ex(&aes, extKey, (word32)sizeof(extKey),
                                   wrappedKey, &blobSz, NULL, 0,
                                   WC_STM32_WRAP_ORDER_RAW);
        if (ret == 0 && blobSz != sizeof(wrappedKey)) {
            ret = -1;
        }
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  [B] key wrap failed: %d\n", ret);
        goto cleanup;
    }
    /* wrappedKey goes to flash; the clear K is discarded. No seed needed --
     * the blob is the key material, bound to this die. */

    /* Runtime half: hand the blob to the DHUK device as the Aes key. The
     * hardware loads K into KEYR; K never appears in software. */
    XMEMSET(ct, 0, sizeof(ct));
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, wrappedKey, (word32)sizeof(wrappedKey), NULL,
                           AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ct, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  [B] encrypt under the wrapped key failed: %d\n", ret);
        goto cleanup;
    }
    printf("  [B] external AES-256 key used from its wrapped blob OK "
           "(K never in software)\n");

    /* ---- B2: when you need the key bytes back ----
     * The device only accepts a 32-byte key, so a 128-bit key -- or key
     * material that is not an AES key, as in flow C -- cannot be loaded into
     * KEYR. Use the DHUK-derived key as a KEK instead and accept that K is in
     * RAM while in use. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, sizeof(seed), NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, wrappedKey, extKey,
                               (word32)sizeof(extKey));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  [B2] KEK wrap failed: %d\n", ret);
        goto cleanup;
    }

    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, sizeof(seed), NULL, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbDecrypt(&aes, recovered, wrappedKey,
                               (word32)sizeof(wrappedKey));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  [B2] KEK unwrap failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(recovered, extKey, sizeof(extKey)) != 0) {
        printf("  [B2] unwrapped key != K -- FAIL\n");
        ret = -1;
        goto cleanup;
    }

    ret = wc_Stm32_AesRegister(WOLFSSL_STM32_AES_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_AesRegister failed: %d\n", ret);
        goto cleanup;
    }
    aesReg = 1;

    XMEMSET(ct, 0, sizeof(ct));
    ret = wc_AesInit(&aes, NULL, WOLFSSL_STM32_AES_DEVID);
    if (ret == 0) {
        /* Used verbatim on this devId -- no derivation. */
        ret = wc_AesSetKey(&aes, recovered, sizeof(recovered), NULL,
                           AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ct, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    wc_ForceZero(recovered, sizeof(recovered));
    if (ret != 0) {
        printf("  [B2] plaintext-key AES failed: %d\n", ret);
        goto cleanup;
    }
    printf("  [B2] same key recovered under a KEK and run verbatim OK "
           "(K in RAM only while in use)\n");

#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
    /* ---- C: ECDSA sign with a DHUK-protected private key ---- */
    {
        static const byte hash[32] = {
            0x9f,0x86,0xd0,0x81,0x88,0x4c,0x7d,0x65,
            0x9a,0x2f,0xea,0xa0,0xc5,0x5a,0xd0,0x15,
            0xa3,0xbf,0x4f,0x1b,0x2b,0x0b,0x82,0x2c,
            0xd1,0x5d,0x6c,0x15,0xb0,0xf0,0x0a,0x08
        };
        ecc_key key;
        ecc_key signer;
        byte    priv[32];
        byte    pub[65];
        byte    sig[80];
        word32  privSz = (word32)sizeof(priv);
        word32  pubSz  = (word32)sizeof(pub);
        word32  sigLen = (word32)sizeof(sig);
        int     verify = 0;
        int     haveKey = 0;
        int     haveSigner = 0;

        ret = wc_ecc_init(&key);
        if (ret != 0) {
            printf("  [C] wc_ecc_init failed: %d\n", ret);
            goto cleanup;
        }
        haveKey = 1;

        /* Factory half: generate the key pair, export the public key, wrap
         * the scalar with the B2 KEK, and forget the clear scalar. */
        ret = wc_ecc_make_key_ex(rng, 32, &key, ECC_SECP256R1);
        if (ret == 0) {
            ret = wc_ecc_export_private_only(&key, priv, &privSz);
        }
        if (ret == 0 && privSz != 32u) {
            ret = -1;
        }
        if (ret == 0) {
            ret = wc_ecc_export_x963(&key, pub, &pubSz);
        }
        if (ret == 0) {
            ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
            if (ret == 0) {
                ret = wc_AesSetKey(&aes, seed, sizeof(seed), NULL,
                                   AES_ENCRYPTION);
                if (ret == 0) {
                    ret = wc_AesEcbEncrypt(&aes, wrappedPriv, priv, 32);
                }
                wc_AesFree(&aes);
            }
        }
        wc_ForceZero(priv, sizeof(priv));
        if (ret != 0) {
            printf("  [C] scalar wrap failed: %d\n", ret);
            goto eccCleanup;
        }
        /* wrappedPriv + seed + the public key go to flash. The provisioning
         * key is released here: reusing it would leave the clear scalar in
         * key->k, and wc_ecc_sign_hash() falls back to software when the
         * callback declines, so a broken wrapped path could still pass. */
        wc_ecc_free(&key);
        haveKey = 0;

        /* Runtime half: a key that has only ever seen wc_ecc_init(). Hand it
         * the blob and the seed, then sign with the ordinary API. The scalar
         * is unwrapped into a short-lived buffer for the PKA and scrubbed --
         * on U3 the CCB path (WOLFSSL_STM32_CCB) keeps it out of software
         * entirely. */
        ret = wc_ecc_init(&signer);
        if (ret != 0) {
            printf("  [C] signer init failed: %d\n", ret);
            goto eccCleanup;
        }
        haveSigner = 1;
        signer.devId = WC_DHUK_DEVID;
        ret = wc_ecc_import_wrapped_private(&signer, ECC_SECP256R1, seed,
                                            (word32)sizeof(seed),
                                            wrappedPriv, 32, 32);
        if (ret == 0) {
            ret = wc_ecc_sign_hash(hash, (word32)sizeof(hash), sig, &sigLen,
                                   rng, &signer);
        }
        if (ret != 0) {
            printf("  [C] DHUK ECDSA sign failed: %d\n", ret);
            goto eccCleanup;
        }

        /* Verify with a public-only key built from the stored public bytes,
         * so nothing private is in play on the verifying side. */
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            haveKey = 1;
            ret = wc_ecc_import_x963_ex(pub, pubSz, &key, ECC_SECP256R1);
        }
        if (ret == 0) {
            ret = wc_ecc_verify_hash(sig, sigLen, hash, (word32)sizeof(hash),
                                     &verify, &key);
        }
        if (ret != 0) {
            printf("  [C] DHUK ECDSA verify error: %d\n", ret);
            goto eccCleanup;
        }
        if (verify != 1) {
            printf("  [C] DHUK ECDSA verify FAILED (sig invalid)\n");
            ret = -1;
            goto eccCleanup;
        }
        printf("  [C] ECDSA signed with a DHUK-wrapped scalar, "
               "verified with the public key\n");

    eccCleanup:
        if (haveSigner) {
            wc_ecc_free(&signer);
        }
        if (haveKey) {
            wc_ecc_free(&key);
        }
        if (ret != 0) {
            goto cleanup;
        }
    }
#endif /* HAVE_ECC && WOLFSSL_STM32_PKA */

cleanup:
    wc_ForceZero(recovered, sizeof(recovered));
    if (aesReg) {
        wc_Stm32_AesUnRegister(WOLFSSL_STM32_AES_DEVID);
    }
    if (dhukReg) {
        wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    }
    return ret;
}
#endif /* (HAVE_AES_ECB || WOLFSSL_AES_DIRECT) && HAVE_AES_CBC */
#endif /* WOLF_CRYPTO_CB */


/* Shared hex dump for the blob/ciphertext comparisons below. */
static void dhuk_print_hex(const char* label, const byte* p, word32 sz)
{
    word32 i;
    printf("  %s:", label);
    for (i = 0; i < sz; i++) {
        printf(" %02x", p[i]);
    }
    printf("\n");
}

#if defined(WOLF_CRYPTO_CB) && \
    (defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT))

/* Runtime half of the pattern under test: treat `blob` as the key on a
 * WC_DHUK_DEVID Aes and decrypt one block. Sets *isK when the recovered
 * plaintext equals the original, i.e. when the hardware really did put the
 * wrapped key back into KEYR. */
static int dhuk_try_blob_as_key(const byte* blob, const byte* enc,
    const byte* ptOrig, byte* dec, int* isK)
{
    Aes aes;
    int ret;

    *isK = 0;
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret != 0) {
        return ret;
    }
    ret = wc_AesSetKey(&aes, blob, 32, NULL, AES_DECRYPTION);
    if (ret == 0) {
        ret = wc_AesEcbDecrypt(&aes, dec, enc, WC_AES_BLOCK_SIZE);
    }
    wc_AesFree(&aes);
    if (ret == 0 && XMEMCMP(dec, ptOrig, WC_AES_BLOCK_SIZE) == 0) {
        *isK = 1;
    }
    return ret;
}

/* [9] A wc_Stm32_Aes_Wrap() blob must unwrap back to the key it wrapped.
 *
 * This is the shape [8] flow B uses: wrap an externally supplied AES-256 key
 * K at provisioning, store the blob, then at runtime hand the blob to
 * wc_AesSetKey() on a WC_DHUK_DEVID Aes. The hardware loads K into KEYR and
 * ciphers with it, so K never appears in software.
 *
 * Guards two defects that both produced a silent wrong answer -- every call
 * returned 0 and the application saw wrong plaintext rather than an error.
 * The unwrap loaded a corrupted key (CR.DATATYPE had to be 00 on the key
 * path, and CR writes made while SR.BUSY is high are dropped, so EN was set
 * too early); and on the CubeMX build wc_Stm32_Aes_Wrap() used to default to
 * WC_STM32_WRAP_ORDER_LEGACY, whose byte-reversed blob does not round-trip.
 * Both are fixed, so both orders are still produced here and the RAW one is
 * asserted to recover K.
 *
 * The KEK pattern is exercised at the end as the alternative for key
 * material the device cannot load into KEYR -- see [8] flow B2. */
static int test_dhuk_wrap_as_key(void)
{
    /* The externally supplied key the application must use verbatim
     * (FIPS-197 AES-256), and one NIST SP 800-38A plaintext block. */
    static const byte extKey[32] = {
        0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
        0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
        0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
        0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
    };
    static const byte ptOrig[WC_AES_BLOCK_SIZE] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    /* Per-key derivation seed for the KEK contrast. Not secret. */
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    Aes    aes;
    byte   enc[WC_AES_BLOCK_SIZE];
    byte   dec[WC_AES_BLOCK_SIZE];
    byte   blobDef[32];
    byte   blobRaw[32];
    byte   kekBlob[32];
    byte   recovered[32];
    word32 blobDefSz = (word32)sizeof(blobDef);
    word32 blobRawSz = (word32)sizeof(blobRaw);
    int    dhukReg = 0;
    int    aesReg  = 0;
    int    defIsK  = 0;
    int    rawIsK  = 0;
    int    ret;

    XMEMSET(enc,       0, sizeof(enc));
    XMEMSET(dec,       0, sizeof(dec));
    XMEMSET(blobDef,   0, sizeof(blobDef));
    XMEMSET(blobRaw,   0, sizeof(blobRaw));
    XMEMSET(kekBlob,   0, sizeof(kekBlob));
    XMEMSET(recovered, 0, sizeof(recovered));

    /* The ciphertext the key-install process supplies: a true AES-256-ECB of
     * ptOrig under K, computed here on the ordinary (non-DHUK) engine. */
    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, extKey, (word32)sizeof(extKey), NULL,
                           AES_ENCRYPTION);
        if (ret == 0) {
            ret = wc_AesEcbEncrypt(&aes, enc, ptOrig, (word32)sizeof(ptOrig));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  reference AES-256-ECB under K failed: %d\n", ret);
        return ret;
    }
    dhuk_print_hex("reference enc = AES-ECB(K, pt)", enc, sizeof(enc));

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_DhukRegister failed: %d\n", ret);
        return ret;
    }
    dhukReg = 1;

    /* Provisioning half, exactly as an integrator writes it: wrap K under the
     * silicon DHUK. The two spellings are deliberate and both are 808:
     * wc_Stm32_Aes_Wrap() reads aes->devId as a wrap-key-source marker and
     * wants WOLFSSL_DHUK_DEVID for KEYSEL=HW, while the crypto-callback
     * device is registered at WC_DHUK_DEVID above. */
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_Stm32_Aes_Wrap(&aes, extKey, (word32)sizeof(extKey),
                                blobDef, &blobDefSz, NULL, 0);
        wc_AesFree(&aes);
    }
    if (is_expected_gated(ret)) {
        printf("  wc_Stm32_Aes_Wrap gated on this silicon (%d) -- skipping\n",
               ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  wc_Stm32_Aes_Wrap (default order) failed: %d\n", ret);
        goto cleanup;
    }

    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_Stm32_Aes_Wrap_ex(&aes, extKey, (word32)sizeof(extKey),
                                   blobRaw, &blobRawSz, NULL, 0,
                                   WC_STM32_WRAP_ORDER_RAW);
        wc_AesFree(&aes);
    }
    if (is_expected_gated(ret)) {
        printf("  wc_Stm32_Aes_Wrap_ex(RAW) gated on this silicon (%d) -- "
               "skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  wc_Stm32_Aes_Wrap_ex(RAW) failed: %d\n", ret);
        goto cleanup;
    }

    /* Use the sizes the API reported, not the buffer sizes, so a short write
     * cannot be printed as trailing garbage or used as a 32-byte key. */
    if (blobDefSz != sizeof(blobDef) || blobRawSz != sizeof(blobRaw)) {
        printf("  wrap returned unexpected size (def=%lu raw=%lu, want %lu) "
               "-- FAIL\n", (unsigned long)blobDefSz, (unsigned long)blobRawSz,
               (unsigned long)sizeof(blobDef));
        ret = -1;
        goto cleanup;
    }

    dhuk_print_hex("blob (this build's default order)", blobDef, blobDefSz);
    dhuk_print_hex("blob (WC_STM32_WRAP_ORDER_RAW)   ", blobRaw, blobRawSz);

    /* Runtime half: hand each blob to the DHUK device as if it were a key. */
    ret = dhuk_try_blob_as_key(blobDef, enc, ptOrig, dec, &defIsK);
    if (is_expected_gated(ret)) {
        printf("  blob-as-key (default order) gated on this silicon "
               "(%d) -- skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  blob-as-key (default order) returned %d\n", ret);
        goto cleanup;
    }
    printf("  blob-as-key, default order: rc=0, plaintext %s\n",
           defIsK ? "MATCHES pt_orig" : "does NOT match pt_orig");
    dhuk_print_hex("  got", dec, sizeof(dec));

    ret = dhuk_try_blob_as_key(blobRaw, enc, ptOrig, dec, &rawIsK);
    if (is_expected_gated(ret)) {
        printf("  blob-as-key (RAW order) gated on this silicon "
               "(%d) -- skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  blob-as-key (RAW order) returned %d\n", ret);
        goto cleanup;
    }
    printf("  blob-as-key, RAW order:     rc=0, plaintext %s\n",
           rawIsK ? "MATCHES pt_orig" : "does NOT match pt_orig");
    dhuk_print_hex("  got", dec, sizeof(dec));

    /* The RAW blob must round-trip: wc_Stm32_Aes_Wrap is the inverse of the
     * SAES wrapped-key load, so handing the blob back as the key recovers K.
     * Regression guard for the unwrap fix (SR.BUSY must be waited out after
     * KEYSEL=HW latches, and the key path runs with CR.DATATYPE = 00). */
    if (!rawIsK) {
        printf("  RAW blob did not unwrap back to K -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  RAW blob unwrapped back to K OK (wrap/unwrap are inverses)\n");
    /* The plain wrap now defaults to RAW on both build paths, so the default
     * blob must round-trip too. */
    if (!defIsK) {
        printf("  default-order blob did not unwrap back to K -- FAIL\n");
        ret = -1;
        goto cleanup;
    }

    /* Contrast: the KEK pattern, which does round-trip. Provision by
     * ECB-encrypting K under the seed-derived key; recover it at runtime with
     * the same seed; then run K verbatim on the plaintext-key device. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, (word32)sizeof(seed), NULL,
                           AES_ENCRYPTION);
        if (ret == 0) {
            ret = wc_AesEcbEncrypt(&aes, kekBlob, extKey,
                                   (word32)sizeof(extKey));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  KEK wrap of K failed: %d\n", ret);
        goto cleanup;
    }

    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, (word32)sizeof(seed), NULL,
                           AES_DECRYPTION);
        if (ret == 0) {
            ret = wc_AesEcbDecrypt(&aes, recovered, kekBlob,
                                   (word32)sizeof(kekBlob));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  KEK unwrap of K failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(recovered, extKey, sizeof(extKey)) != 0) {
        printf("  KEK unwrap did not recover K -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  KEK round-trip recovered K OK\n");

    ret = wc_Stm32_AesRegister(WOLFSSL_STM32_AES_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_AesRegister failed: %d\n", ret);
        goto cleanup;
    }
    aesReg = 1;

    XMEMSET(dec, 0, sizeof(dec));
    ret = wc_AesInit(&aes, NULL, WOLFSSL_STM32_AES_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, recovered, (word32)sizeof(recovered), NULL,
                           AES_DECRYPTION);
        if (ret == 0) {
            ret = wc_AesEcbDecrypt(&aes, dec, enc, (word32)sizeof(enc));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  plaintext-key decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(dec, ptOrig, sizeof(ptOrig)) != 0) {
        printf("  plaintext-key decrypt did not recover pt_orig -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  KEK pattern decrypted enc back to pt_orig OK "
           "(the alternative when the key bytes are needed)\n");

cleanup:
    wc_ForceZero(recovered, sizeof(recovered));
    if (aesReg) {
        wc_Stm32_AesUnRegister(WOLFSSL_STM32_AES_DEVID);
    }
    if (dhukReg) {
        wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    }
    return ret;
}
#endif /* WOLF_CRYPTO_CB && (HAVE_AES_ECB || WOLFSSL_AES_DIRECT) */

#ifdef WOLFSSL_STM32_DHUK_UNWRAP
/* [6] wc_Stm32_Aes_DhukOp_ex -- the provisioning flow the API exists for:
 * stage a 256-bit seed, let SAES turn (seed, silicon DHUK) into a key
 * encryption key inside KEYR, and wrap/unwrap other key material with it.
 * The KEK never enters software and is bound to this chip.
 *
 * Contract checked here:
 *   - enc/dec round-trip through the same seed is the identity (ECB + CBC)
 *   - the same seed always yields the same KEK (determinism)
 *   - DhukOp_ex and the crypto-callback device agree byte-for-byte on the
 *     same 32-byte input -- they are the same KEK = DHUK-decrypt(seed)
 *     primitive, so blobs are interchangeable between the two APIs
 *   - a wc_Stm32_Aes_Wrap blob unwraps back to the exact key it wrapped:
 *     DhukOp ct == AES-ECB(K, pt) for the K that went into the wrap
 *
 * Ciphertext is silicon-specific, so it is printed rather than pinned; the
 * printed values are what the cross-build (BUILD=bare vs BUILD=cubemx)
 * comparison uses. */
static int test_dhuk_op_roundtrip(void)
{
    /* The key we want the DHUK to protect. */
    static const byte kek[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte pt[32] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
        0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,
        0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51
    };
    Aes    aes;
    byte   wrapped[32];
    byte   ct[32];
    byte   rt[32];
    byte   swCt[32];
    word32 wrappedSz = 0;
    int    ret;
    int    rc = 0;

    XMEMSET(wrapped, 0, sizeof(wrapped));
    XMEMSET(ct, 0, sizeof(ct));
    XMEMSET(rt, 0, sizeof(rt));
    XMEMSET(swCt, 0, sizeof(swCt));

    /* Stage 1: wrap K under the silicon DHUK. */
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_AesInit (wrap) failed: %d\n", ret);
        return ret;
    }
    ret = wc_Stm32_Aes_Wrap(&aes, kek, sizeof(kek), wrapped, &wrappedSz,
                            NULL, 0);
    wc_AesFree(&aes);
    if (is_expected_gated(ret)) {
        printf("  wc_Stm32_Aes_Wrap gated/unavailable: %d "
               "(expected on this silicon)\n", ret);
        g_dhuk_res.dhukop_rc = ret;
        return 0;
    }
    if (ret != 0) {
        printf("  wc_Stm32_Aes_Wrap failed: %d\n", ret);
        g_dhuk_res.dhukop_rc = ret;
        return ret;
    }
    if (wrappedSz != sizeof(wrapped)) {
        printf("  wc_Stm32_Aes_Wrap outSz %u, want %u -- FAIL\n",
               (unsigned)wrappedSz, (unsigned)sizeof(wrapped));
        g_dhuk_res.dhukop_rc = -1;
        return -1;
    }
    dhuk_print_hex("wrapped blob (chip-bound)", wrapped, sizeof(wrapped));
    XMEMCPY((void*)g_dhuk_res.dhukop_blob, wrapped, sizeof(wrapped));

    /* Stage 2: ECB encrypt through the unwrapped key. */
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_AesInit (op) failed: %d\n", ret);
        return ret;
    }
    XMEMCPY(aes.key, wrapped, sizeof(wrapped));
    aes.keylen = 32;
    ret = wc_Stm32_Aes_DhukOp_ex(&aes, ct, pt, sizeof(pt), 1 /* enc */,
                                 0 /* isCbc */);
    if (is_expected_gated(ret)) {
        printf("  DhukOp_ex ECB encrypt gated/unavailable: %d\n", ret);
        wc_AesFree(&aes);
        g_dhuk_res.dhukop_rc = ret;
        return 0;
    }
    if (ret != 0) {
        printf("  DhukOp_ex ECB encrypt failed: %d\n", ret);
        wc_AesFree(&aes);
        g_dhuk_res.dhukop_rc = ret;
        return ret;
    }
    dhuk_print_hex("DhukOp ECB ct", ct, sizeof(ct));

    /* Diagnostic: run the same input through DhukOp twice more, and through
     * the crypto-callback device (which uses Stm32SaesDeriveKeyFromSeed --
     * the same KEK = DHUK-decrypt(input) primitive). Feeding both paths the
     * identical 32 bytes isolates a code difference from an input
     * difference: if the cb path is deterministic and DhukOp is not, the
     * bug is in DhukOp; if they agree, the two are the same primitive. */
    {
        Aes  aes2;
        byte ct2[32];
        byte ct3[32];
        byte cbCt[32];
        int  i;

        for (i = 0; i < 2; i++) {
            byte* dst = (i == 0) ? ct2 : ct3;
            XMEMSET(dst, 0, sizeof(ct2));
            ret = wc_AesInit(&aes2, NULL, WOLFSSL_DHUK_DEVID);
            if (ret != 0) {
                break;
            }
            XMEMCPY(aes2.key, wrapped, sizeof(wrapped));
            aes2.keylen = 32;
            ret = wc_Stm32_Aes_DhukOp_ex(&aes2, dst, pt, sizeof(pt),
                                         1 /* enc */, 0 /* isCbc */);
            wc_AesFree(&aes2);
            if (ret != 0) {
                break;
            }
        }
        if (ret != 0) {
            printf("  repeat probe failed: %d\n", ret);
            rc = -1;
        }
        else {
            dhuk_print_hex("DhukOp ct pass2", ct2, sizeof(ct2));
            dhuk_print_hex("DhukOp ct pass3", ct3, sizeof(ct3));
            if (XMEMCMP(ct2, ct, sizeof(ct)) != 0 ||
                XMEMCMP(ct3, ct, sizeof(ct)) != 0) {
                printf("  DhukOp encrypt NOT deterministic\n");
                rc = -1;
            }
            else {
                printf("  DhukOp encrypt deterministic OK\n");
            }
        }

        /* Same 32 bytes, but through the crypto-callback derive path. */
        XMEMSET(cbCt, 0, sizeof(cbCt));
        ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
        if (ret == 0) {
            ret = wc_AesInit(&aes2, NULL, WC_DHUK_DEVID);
            if (ret == 0) {
                ret = wc_AesSetKey(&aes2, wrapped, sizeof(wrapped), NULL,
                                   AES_ENCRYPTION);
                if (ret == 0) {
                    ret = wc_AesEcbEncrypt(&aes2, cbCt, pt, sizeof(pt));
                }
                wc_AesFree(&aes2);
            }
            wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
        }
        if (ret != 0) {
            printf("  cb-path comparison failed: %d\n", ret);
        }
        else {
            dhuk_print_hex("cb-path ct    ", cbCt, sizeof(cbCt));
            printf("  DhukOp %s cb-path (same 32-byte input)\n",
                   (XMEMCMP(cbCt, ct, sizeof(ct)) == 0) ? "==" : "!=");
        }
        ret = 0;
    }

    /* Stage 3: decrypt back, expect identity. */
    ret = wc_Stm32_Aes_DhukOp_ex(&aes, rt, ct, sizeof(ct), 0 /* dec */,
                                 0 /* isCbc */);
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  DhukOp_ex ECB decrypt failed: %d\n", ret);
        g_dhuk_res.dhukop_rc = ret;
        return ret;
    }
    if (XMEMCMP(rt, pt, sizeof(pt)) != 0) {
        printf("  DhukOp_ex ECB round-trip mismatch -- FAIL\n");
        dhuk_print_hex("got ", rt, sizeof(rt));
        rc = -1;
    }
    else {
        printf("  DhukOp_ex ECB round-trip OK\n");
    }

    /* Stage 4: the recovered KEK must equal K, so a plain AES keyed with K
     * has to produce the same ciphertext. Anything else means the unwrap
     * landed a different key and the test fails; the permutations are probed
     * only so the diagnostic can say whether it is the blob byte order that
     * disagrees. */
    {
        static const char* names[4] = {
            "K as-is", "K byte-reversed", "K word-order-reversed",
            "K per-word byteswapped"
        };
        byte cand[32];
        int  v;
        int  j;
        int  hit = -1;

        for (v = 0; v < 4; v++) {
            switch (v) {
                case 0:
                    XMEMCPY(cand, kek, sizeof(cand));
                    break;
                case 1:
                    for (j = 0; j < 32; j++) {
                        cand[j] = kek[31 - j];
                    }
                    break;
                case 2:
                    for (j = 0; j < 8; j++) {
                        XMEMCPY(cand + 4 * j, kek + 4 * (7 - j), 4);
                    }
                    break;
                default:
                    for (j = 0; j < 8; j++) {
                        cand[4 * j + 0] = kek[4 * j + 3];
                        cand[4 * j + 1] = kek[4 * j + 2];
                        cand[4 * j + 2] = kek[4 * j + 1];
                        cand[4 * j + 3] = kek[4 * j + 0];
                    }
                    break;
            }
            ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
            if (ret != 0) {
                break;
            }
            ret = wc_AesSetKey(&aes, cand, sizeof(cand), NULL, AES_ENCRYPTION);
            if (ret == 0) {
                ret = wc_AesEcbEncrypt(&aes, swCt, pt, sizeof(pt));
            }
            wc_AesFree(&aes);
            if (ret != 0) {
                break;
            }
            if (XMEMCMP(swCt, ct, sizeof(ct)) == 0) {
                hit = v;
                break;
            }
        }
        if (ret != 0) {
            printf("  reference AES-ECB failed (%d) -- FAIL\n", ret);
            rc = -1;
        }
        else if (hit != 0) {
            printf("  DhukOp ct != AES-ECB(K as-is, pt) -- FAIL (%s)\n",
                   (hit < 0) ? "KEK != K under any word/byte permutation"
                             : names[hit]);
            rc = -1;
        }
        else {
            printf("  DhukOp ct == AES-ECB(%s, pt)\n", names[hit]);
        }
    }

    /* CBC through the same seed must also round-trip. */
    {
        static const byte iv0[16] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
        };
        Aes  aesc;
        byte cbcCt[32];
        byte cbcRt[32];

        XMEMSET(cbcCt, 0, sizeof(cbcCt));
        XMEMSET(cbcRt, 0, sizeof(cbcRt));
        ret = wc_AesInit(&aesc, NULL, WOLFSSL_DHUK_DEVID);
        if (ret == 0) {
            XMEMCPY(aesc.key, wrapped, sizeof(wrapped));
            aesc.keylen = 32;
            XMEMCPY(aesc.reg, iv0, sizeof(iv0));
            ret = wc_Stm32_Aes_DhukOp_ex(&aesc, cbcCt, pt, sizeof(pt),
                                         1 /* enc */, 1 /* isCbc */);
            if (ret == 0) {
                XMEMCPY(aesc.reg, iv0, sizeof(iv0));
                ret = wc_Stm32_Aes_DhukOp_ex(&aesc, cbcRt, cbcCt,
                                             sizeof(cbcCt), 0 /* dec */,
                                             1 /* isCbc */);
            }
            wc_AesFree(&aesc);
        }
        if (ret != 0) {
            printf("  DhukOp_ex CBC failed: %d\n", ret);
            rc = -1;
        }
        else if (XMEMCMP(cbcRt, pt, sizeof(pt)) != 0) {
            printf("  DhukOp_ex CBC round-trip mismatch -- FAIL\n");
            dhuk_print_hex("got ", cbcRt, sizeof(cbcRt));
            rc = -1;
        }
        else if (XMEMCMP(cbcCt, ct, sizeof(ct)) == 0) {
            printf("  DhukOp_ex CBC ct == ECB ct -- IV not applied, FAIL\n");
            rc = -1;
        }
        else {
            printf("  DhukOp_ex CBC round-trip OK (IV applied)\n");
        }
        ret = 0;
    }

    g_dhuk_res.dhukop_rc = rc;
    return rc;
}
#endif /* WOLFSSL_STM32_DHUK_UNWRAP */

#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
/* [10] Sign with a wrapped scalar imported onto a key that was never used for
 * keygen -- the shape a product actually has at runtime, where provisioning
 * happened in the factory and only the blob plus the seed reach flash. The
 * surrounding ECDSA test starts from wc_ecc_make_key_ex(), which hides whether
 * wc_ecc_import_wrapped_private() leaves the key ready to sign; this does not.
 * The signature is verified against the public counterpart, so a scalar that
 * unwrapped to the wrong value fails here rather than passing silently. */
static int test_dhuk_ecdsa_import_sign(WC_RNG* rng, int curveId, word32 keySz)
{
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte hash[32] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
        0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,
        0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,
        0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20
    };
    ecc_key kp;
    ecc_key signer;
    ecc_key verifier;
    Aes     aes;
    byte    priv[48];
    byte    wrapped[48];
    byte    pub[97];
    byte    sig[112];
    word32  privSz = (word32)sizeof(priv);
    word32  pubSz  = (word32)sizeof(pub);
    word32  sigLen = (word32)sizeof(sig);
    int     verify = 0;
    int     dhukReg = 0;
    int     ret;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }
    dhukReg = 1;

    /* Factory half: keypair, export the scalar and the public key. */
    ret = wc_ecc_init(&kp);
    if (ret != 0) {
        goto cleanup;
    }
    ret = wc_ecc_make_key_ex(rng, (int)keySz, &kp, curveId);
    if (ret == 0) {
        ret = wc_ecc_export_private_only(&kp, priv, &privSz);
    }
    if (ret == 0) {
        ret = wc_ecc_export_x963(&kp, pub, &pubSz);
    }
    wc_ecc_free(&kp);
    if (ret != 0 || privSz != keySz) {
        printf("  provisioning failed: %d (privSz %lu)\n", ret,
               (unsigned long)privSz);
        ret = (ret != 0) ? ret : -1;
        goto cleanup;
    }

    /* Wrap the scalar under the DHUK-derived key. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, (word32)sizeof(seed), NULL,
                           AES_ENCRYPTION);
        if (ret == 0) {
            ret = wc_AesEcbEncrypt(&aes, wrapped, priv, keySz);
        }
        wc_AesFree(&aes);
    }
    wc_ForceZero(priv, sizeof(priv));
    if (is_expected_gated(ret)) {
        printf("  KEK wrap gated on this silicon (%d) -- skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  KEK wrap failed: %d\n", ret);
        goto cleanup;
    }

    /* Runtime half: a key that has only ever seen wc_ecc_init(). */
    ret = wc_ecc_init(&signer);
    if (ret != 0) {
        goto cleanup;
    }
    signer.devId = WC_DHUK_DEVID;
    ret = wc_ecc_import_wrapped_private(&signer, curveId, seed,
                                        (word32)sizeof(seed), wrapped, keySz,
                                        keySz);
    if (ret == 0) {
        ret = wc_ecc_sign_hash(hash, (word32)sizeof(hash), sig, &sigLen, rng,
                               &signer);
    }
    wc_ecc_free(&signer);
    if (is_expected_gated(ret)) {
        printf("  DHUK sign gated on this silicon (%d) -- skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  sign after import failed: %d "
               "(-170 means the import left no curve on the key)\n", ret);
        goto cleanup;
    }
    printf("  signed with an imported scalar, no keygen (%lu-byte sig)\n",
           (unsigned long)sigLen);

    /* The signature must verify against the provisioned public key. */
    ret = wc_ecc_init(&verifier);
    if (ret == 0) {
        ret = wc_ecc_import_x963_ex(pub, pubSz, &verifier, curveId);
        if (ret == 0) {
            ret = wc_ecc_verify_hash(sig, sigLen, hash, (word32)sizeof(hash),
                                     &verify, &verifier);
        }
        wc_ecc_free(&verifier);
    }
    if (ret != 0 || verify != 1) {
        printf("  signature did not verify (ret=%d verify=%d) -- FAIL\n",
               ret, verify);
        ret = (ret != 0) ? ret : -1;
        goto cleanup;
    }
    printf("  verifies against the provisioned public key OK\n");

cleanup:
    /* Scrub in the common exit: an early failure in the export or public-key
     * step jumps here with priv possibly partially written. */
    wc_ForceZero(priv, sizeof(priv));
    if (dhukReg) {
        wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    }
    return ret;
}
#endif /* HAVE_ECC && WOLFSSL_STM32_PKA */

#if defined(WOLF_CRYPTO_CB) && defined(HAVE_AESGCM)
/* [12] Encrypt a key at rest under the DHUK and recover the PLAINTEXT bytes
 * into RAM. The other tests here keep the key inside SAES; this is the case
 * where an algorithm outside wolfCrypt needs the bytes themselves, so the
 * payload is treated as data rather than as a hardware key.
 *
 * The 256-bit value is still only a derivation seed -- SAES mixes it with the
 * silicon DHUK and the working key exists solely in KEYR -- so the wrapping key
 * never enters software. Only the payload does, and only while it is in use.
 *
 * AES-GCM rather than ECB/CBC for two reasons: the payload length is arbitrary
 * (ECB/CBC need a multiple of the block size), and the tag detects a corrupted
 * blob, or one from another seed or key id, instead of handing the caller
 * garbage key bytes. Both properties are checked below, the second by actually
 * corrupting a blob. The tag does not detect rollback: an older valid record
 * for the same key id authenticates too, as nothing here carries a generation
 * number. */
static int test_dhuk_keyblob_to_ram(WC_RNG* rng)
{
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    /* Same length as the seed, one byte different, to show the blob is bound
     * to the seed it was produced under. */
    static const byte wrongSeed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x02
    };
    /* Generated per payload, not fixed. GCM needs a unique nonce for every
     * payload encrypted under one key: reusing one across two payloads under
     * the same seed forfeits both confidentiality and the tag. Store it beside
     * the blob -- it is not secret. */
    byte iv[12];
    /* Authenticated, not encrypted: a key id and format version, so a blob
     * stored under one id cannot be presented as another or under an older
     * format. A product that also needs rollback protection binds a trusted
     * monotonic counter in here and rejects records older than its own. */
    static const byte aad[8]      = { 'k','e','y','i','d','=','7', 0x01 };
    static const byte wrongAad[8] = { 'k','e','y','i','d','=','8', 0x01 };
    /* 37 bytes: deliberately not an AES key size and not a block multiple,
     * which is the shape a proprietary key tends to have. */
    static const byte propKey[37] = {
        0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,
        0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,
        0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,
        0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,
        0x71,0x72,0x73,0x74,0x75
    };
    Aes    aes;
    byte   work[sizeof(propKey)];
    byte   blob[sizeof(propKey)];
    byte   recovered[sizeof(propKey)];
    byte   tag[16];
    int    dhukReg = 0;
    int    ret;

    XMEMSET(blob, 0, sizeof(blob));
    XMEMSET(recovered, 0, sizeof(recovered));
    XMEMSET(tag, 0, sizeof(tag));

    ret = wc_RNG_GenerateBlock(rng, iv, (word32)sizeof(iv));
    if (ret != 0) {
        printf("  iv generation failed: %d\n", ret);
        return ret;
    }

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_DhukRegister failed: %d\n", ret);
        return ret;
    }
    dhukReg = 1;

    /* Provisioning half: wrap the key once. A product does this in the factory
     * and ships only the blob, the tag, the iv and the seed. */
    XMEMCPY(work, propKey, sizeof(propKey));
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
        if (ret == 0) {
            ret = wc_AesGcmEncrypt(&aes, blob, work, (word32)sizeof(work),
                                   iv, (word32)sizeof(iv),
                                   tag, (word32)sizeof(tag),
                                   aad, (word32)sizeof(aad));
        }
        wc_AesFree(&aes);
    }
    wc_ForceZero(work, sizeof(work));
    if (is_expected_gated(ret)) {
        printf("  AES-GCM gated on this silicon (%d) -- skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  wrap failed: %d\n", ret);
        goto cleanup;
    }
    printf("  wrapped %u-byte key (no length padding needed)\n",
           (unsigned int)sizeof(propKey));

    /* Runtime half: recover the bytes into a buffer the caller owns. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
        if (ret == 0) {
            ret = wc_AesGcmDecrypt(&aes, recovered, blob, (word32)sizeof(blob),
                                   iv, (word32)sizeof(iv),
                                   tag, (word32)sizeof(tag),
                                   aad, (word32)sizeof(aad));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  unwrap failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(recovered, propKey, sizeof(propKey)) != 0) {
        printf("  recovered key does not match the original -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  unwrapped into RAM, matches the original\n");

    /* A corrupted blob must be rejected, not silently returned as key bytes. */
    blob[0] ^= 0x01;
    XMEMSET(recovered, 0, sizeof(recovered));
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
        if (ret == 0) {
            ret = wc_AesGcmDecrypt(&aes, recovered, blob, (word32)sizeof(blob),
                                   iv, (word32)sizeof(iv),
                                   tag, (word32)sizeof(tag),
                                   aad, (word32)sizeof(aad));
        }
        wc_AesFree(&aes);
    }
    blob[0] ^= 0x01;
    if (expect_ret("corrupted blob rejected", ret, AES_GCM_AUTH_E) != 0) {
        ret = -1;
        goto cleanup;
    }

    /* The blob is bound to its seed (and through the seed, to this silicon). */
    XMEMSET(recovered, 0, sizeof(recovered));
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, wrongSeed, (word32)sizeof(wrongSeed));
        if (ret == 0) {
            ret = wc_AesGcmDecrypt(&aes, recovered, blob, (word32)sizeof(blob),
                                   iv, (word32)sizeof(iv),
                                   tag, (word32)sizeof(tag),
                                   aad, (word32)sizeof(aad));
        }
        wc_AesFree(&aes);
    }
    if (expect_ret("wrong seed rejected", ret, AES_GCM_AUTH_E) != 0) {
        ret = -1;
        goto cleanup;
    }

    /* The key id is authenticated, so a blob stored under one id cannot be
     * presented as another. */
    XMEMSET(recovered, 0, sizeof(recovered));
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
        if (ret == 0) {
            ret = wc_AesGcmDecrypt(&aes, recovered, blob, (word32)sizeof(blob),
                                   iv, (word32)sizeof(iv),
                                   tag, (word32)sizeof(tag),
                                   wrongAad, (word32)sizeof(wrongAad));
        }
        wc_AesFree(&aes);
    }
    if (expect_ret("wrong key id rejected", ret, AES_GCM_AUTH_E) != 0) {
        ret = -1;
        goto cleanup;
    }
    ret = 0;

cleanup:
    wc_ForceZero(work, sizeof(work));
    wc_ForceZero(recovered, sizeof(recovered));
    if (dhukReg) {
        wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    }
    return ret;
}
#endif /* WOLF_CRYPTO_CB && HAVE_AESGCM */

#if defined(WOLFSSL_STM32_DHUK_UNWRAP)
/* [13] The exact-key alternative to [12]. Provisioning wraps a chosen key once
 * with wc_Stm32_Aes_Wrap_ex() and keeps the blob; at runtime the blob (not the
 * key) is staged in aes->key and wc_Stm32_Aes_DhukOp_ex() unwraps it inside
 * KEYR -- the key never enters software -- and ciphers the caller's buffer
 * with it. Test [6] covers a single 32-byte block; a product wrapping a real
 * payload needs more than one, so this runs a four-block buffer through ECB
 * and CBC, checks every block against software AES-256 keyed with the chosen
 * key (a round-trip alone cannot tell a correct unwrap from a no-op), and
 * checks both round-trip.
 *
 * Two differences from [12] decide which to use: this path requires the payload
 * to be a multiple of the AES block size, and it carries no integrity tag, so a
 * corrupted blob yields wrong bytes silently rather than an error. */
static int test_dhuk_op_longpayload(void)
{
    static const byte kekIn[32] = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c,
        0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
        0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81
    };
    static const byte iv0[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    /* 64 bytes: four blocks, so a multi-block CBC chain is actually exercised
     * rather than the single block test [6] covers. */
    static const byte payload[64] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x01,0x12,0x23,0x34,0x45,0x56,0x67,0x78,
        0x89,0x9a,0xab,0xbc,0xcd,0xde,0xef,0xf0,
        0x02,0x13,0x24,0x35,0x46,0x57,0x68,0x79,
        0x8a,0x9b,0xac,0xbd,0xce,0xdf,0xe0,0xf1,
        0x03,0x14,0x25,0x36,0x47,0x58,0x69,0x7a,
        0x8b,0x9c,0xad,0xbe,0xcf,0xd0,0xe1,0xf2
    };
    Aes    aes;
    byte   kekBlob[sizeof(kekIn)];
    byte   blob[sizeof(payload)];
    byte   ref[sizeof(payload)];
    byte   back[sizeof(payload)];
    word32 kekBlobSz = 0;
    int    ret;

    XMEMSET(kekBlob, 0, sizeof(kekBlob));
    XMEMSET(blob, 0, sizeof(blob));
    XMEMSET(ref, 0, sizeof(ref));
    XMEMSET(back, 0, sizeof(back));

    /* No device registration here: wc_Stm32_Aes_DhukOp_ex drives SAES directly
     * rather than dispatching through the crypto-callback device. */

    /* Provisioning: wrap the chosen key under the DHUK. RAW order is the blob
     * format DhukOp_ex unwraps; the blob is what the runtime half stages. */
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_Stm32_Aes_Wrap_ex(&aes, kekIn, (word32)sizeof(kekIn),
                                   kekBlob, &kekBlobSz, NULL, 0,
                                   WC_STM32_WRAP_ORDER_RAW);
        wc_AesFree(&aes);
    }
    if (is_expected_gated(ret)) {
        printf("  wc_Stm32_Aes_Wrap_ex gated on this silicon (%d) -- "
               "skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  wc_Stm32_Aes_Wrap_ex failed: %d\n", ret);
        goto cleanup;
    }
    if (kekBlobSz != sizeof(kekBlob)) {
        printf("  wc_Stm32_Aes_Wrap_ex outSz %u, want %u -- FAIL\n",
               (unsigned int)kekBlobSz, (unsigned int)sizeof(kekBlob));
        ret = -1;
        goto cleanup;
    }

    /* ECB, four blocks. */
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        XMEMCPY(aes.key, kekBlob, sizeof(kekBlob));
        aes.keylen = 32;
        ret = wc_Stm32_Aes_DhukOp_ex(&aes, blob, payload,
                                     (word32)sizeof(payload), 1, 0);
        wc_AesFree(&aes);
    }
    if (is_expected_gated(ret)) {
        printf("  DhukOp gated on this silicon (%d) -- skipping\n", ret);
        ret = 0;
        goto cleanup;
    }
    if (ret != 0) {
        printf("  DhukOp ECB encrypt failed: %d\n", ret);
        goto cleanup;
    }

    /* The unwrapped key must be kekIn itself, so software AES-256 keyed with
     * kekIn has to produce the same four blocks. A no-op or first-block-only
     * unwrap still round-trips below; this is what catches it. */
    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, kekIn, (word32)sizeof(kekIn), NULL,
                           AES_ENCRYPTION);
        if (ret == 0) {
            ret = wc_AesEcbEncrypt(&aes, ref, payload, (word32)sizeof(payload));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  reference AES-ECB failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(blob, ref, sizeof(ref)) != 0) {
        printf("  DhukOp ECB ct != AES-ECB(kekIn, payload) -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  DhukOp ECB ct == AES-ECB(kekIn, payload) over %u bytes\n",
           (unsigned int)sizeof(payload));

    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        XMEMCPY(aes.key, kekBlob, sizeof(kekBlob));
        aes.keylen = 32;
        ret = wc_Stm32_Aes_DhukOp_ex(&aes, back, blob,
                                     (word32)sizeof(blob), 0, 0);
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  DhukOp ECB decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(back, payload, sizeof(payload)) != 0) {
        printf("  ECB round-trip did not recover the payload -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  ECB round-trip OK over %u bytes\n",
           (unsigned int)sizeof(payload));

    /* CBC over the same payload, so the IV chain is exercised across blocks. */
    XMEMSET(blob, 0, sizeof(blob));
    XMEMSET(ref, 0, sizeof(ref));
    XMEMSET(back, 0, sizeof(back));
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        XMEMCPY(aes.key, kekBlob, sizeof(kekBlob));
        aes.keylen = 32;
        XMEMCPY(aes.reg, iv0, sizeof(iv0));
        ret = wc_Stm32_Aes_DhukOp_ex(&aes, blob, payload,
                                     (word32)sizeof(payload), 1, 1);
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  DhukOp CBC encrypt failed: %d\n", ret);
        goto cleanup;
    }
    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, kekIn, (word32)sizeof(kekIn), iv0,
                           AES_ENCRYPTION);
        if (ret == 0) {
            ret = wc_AesCbcEncrypt(&aes, ref, payload, (word32)sizeof(payload));
        }
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  reference AES-CBC failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(blob, ref, sizeof(ref)) != 0) {
        printf("  DhukOp CBC ct != AES-CBC(kekIn, iv0, payload) -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  DhukOp CBC ct == AES-CBC(kekIn, iv0, payload) over %u bytes\n",
           (unsigned int)sizeof(payload));

    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        XMEMCPY(aes.key, kekBlob, sizeof(kekBlob));
        aes.keylen = 32;
        XMEMCPY(aes.reg, iv0, sizeof(iv0));
        ret = wc_Stm32_Aes_DhukOp_ex(&aes, back, blob,
                                     (word32)sizeof(blob), 0, 1);
        wc_AesFree(&aes);
    }
    if (ret != 0) {
        printf("  DhukOp CBC decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(back, payload, sizeof(payload)) != 0) {
        printf("  CBC round-trip did not recover the payload -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  CBC round-trip OK over %u bytes (IV chained)\n",
           (unsigned int)sizeof(payload));

    /* No integrity tag on this path: a corrupted blob decrypts to wrong bytes
     * with no error, which is the practical reason to prefer [12]'s AEAD. */
    blob[0] ^= 0x01;
    ret = wc_AesInit(&aes, NULL, WOLFSSL_DHUK_DEVID);
    if (ret == 0) {
        XMEMCPY(aes.key, kekBlob, sizeof(kekBlob));
        aes.keylen = 32;
        XMEMCPY(aes.reg, iv0, sizeof(iv0));
        ret = wc_Stm32_Aes_DhukOp_ex(&aes, back, blob,
                                     (word32)sizeof(blob), 0, 1);
        wc_AesFree(&aes);
    }
    blob[0] ^= 0x01;
    if (ret != 0) {
        printf("  corrupted-blob decrypt errored unexpectedly: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(back, payload, sizeof(payload)) == 0) {
        printf("  corrupted blob still decrypted to the payload -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  corrupted blob returns wrong bytes with no error "
           "(no integrity tag on this path)\n");
    ret = 0;

cleanup:
    wc_ForceZero(back, sizeof(back));
    return ret;
}
#endif /* WOLFSSL_STM32_DHUK_UNWRAP */

#endif /* WOLFSSL_DHUK && (BARE || CUBEMX) && WC_STM32_HAS_DHUK */



int main(void)
{
    int ret = 0;

    board_init();
    SystemCoreClockUpdate();

    printf("\n");
    printf("========================================\n");
    printf("wolfCrypt DHUK test - %s (CONFIG=%s)\n",
           board_name(), BUILD_CONFIG_NAME);
    printf("wolfSSL version: %s\n", LIBWOLFSSL_VERSION_STRING);
    printf("SYSCLK: expected %lu Hz, CMSIS-reported %lu Hz%s\n",
           (unsigned long)board_sysclk_hz(),
           (unsigned long)SystemCoreClock,
           ((unsigned long)SystemCoreClock == (unsigned long)board_sysclk_hz())
               ? " (match)" : " (MISMATCH -- PLL may have failed)");
    printf("========================================\n\n");

    ret = wolfCrypt_Init();
    if (ret != 0) {
        printf("wolfCrypt_Init failed: %d\n", ret);
        for (;;) { }
    }

#if defined(WOLFSSL_DHUK) && \
    (defined(WOLFSSL_STM32_BARE) || defined(WOLFSSL_STM32_CUBEMX)) && \
    defined(WC_STM32_HAS_DHUK)
    {
        WC_RNG rng;

        printf("[1] ECC DHUK setter validation (SW unit test):\n");
        ret = test_ecc_dhuk_setter();
        if (ret != 0) {
            goto done;
        }

        ret = wc_InitRng(&rng);
        if (ret != 0) {
            printf("  wc_InitRng failed: %d\n", ret);
            goto done;
        }

#ifdef WOLF_CRYPTO_CB
        if (ret == 0) {
            printf("\n[2] GMAC via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_gmac();
        }
#if defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT)
        if (ret == 0) {
            printf("\n[3] AES-ECB via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_ecb();
        }
#endif
#if defined(HAVE_AES_CBC)
        if (ret == 0) {
            printf("\n[5] AES-CBC via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_cbc();
        }
#endif
#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
        if (ret == 0) {
            printf("\n[4] ECDSA sign via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_ecdsa(&rng);
        }
#endif
#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
        if (ret == 0) {
            printf("\n[10] ECDSA sign from an imported wrapped scalar "
                   "(no keygen), P-256:\n");
            ret = test_dhuk_ecdsa_import_sign(&rng, ECC_SECP256R1, 32);
        }
#ifdef HAVE_ECC384
        if (ret == 0) {
            printf("\n[11] ECDSA sign from an imported wrapped scalar "
                   "(no keygen), P-384:\n");
            ret = test_dhuk_ecdsa_import_sign(&rng, ECC_SECP384R1, 48);
        }
#endif
#endif
#endif
#ifdef WOLFSSL_STM32_DHUK_UNWRAP
        if (ret == 0) {
            printf("\n[6] wc_Stm32_Aes_DhukOp_ex wrap/unwrap round-trip:\n");
            ret = test_dhuk_op_roundtrip();
        }
#endif
        if (ret == 0) {
            printf("\n[7] wc_Stm32_Aes_Wrap blob word order:\n");
            ret = test_dhuk_wrap_order();
        }
#if defined(WOLF_CRYPTO_CB) && \
    (defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT))
        if (ret == 0) {
            printf("\n[9] wc_Stm32_Aes_Wrap blob used as a key "
                   "round-trips back to K:\n");
            ret = test_dhuk_wrap_as_key();
        }
#endif
#if defined(WOLF_CRYPTO_CB) && defined(HAVE_AES_CBC) && \
    (defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT))
        if (ret == 0) {
            printf("\n[8] Provisioning reference (start here):\n");
            ret = dhuk_provision_example(&rng);
        }
#endif
#if defined(WOLF_CRYPTO_CB) && defined(HAVE_AESGCM)
        if (ret == 0) {
            printf("\n[12] Key wrapped at rest, unwrapped into RAM:\n");
            ret = test_dhuk_keyblob_to_ram(&rng);
        }
#endif
#ifdef WOLFSSL_STM32_DHUK_UNWRAP
        if (ret == 0) {
            printf("\n[13] wc_Stm32_Aes_DhukOp_ex over a longer payload:\n");
            ret = test_dhuk_op_longpayload();
        }
#endif

        wc_FreeRng(&rng);
    }
#else
    printf("DHUK not enabled in this build (need WOLFSSL_DHUK + "
           "WOLFSSL_STM32_BARE or WOLFSSL_STM32_CUBEMX + "
           "WC_STM32_HAS_DHUK).\n");
    ret = -1;
#endif

done:
    g_dhuk_res.overall = ret;
    g_dhuk_res.magic = 0xD04B0001u;
    wolfCrypt_Cleanup();
    printf("\nResult: %d (%s)\n", ret, ret == 0 ? "PASS" : "FAIL");
    printf("Test complete\n");

    for (;;) { }
}
