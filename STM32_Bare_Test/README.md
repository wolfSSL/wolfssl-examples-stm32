# STM32_Bare_Test

HAL-free wolfCrypt validation for STM32 boards. Same source tree builds
across multiple boards via `BOARD=` and across multiple wolfSSL build
flavors via `CONFIG=`.

## Boards wired up in this example

Status from the 2026-06-03 multi-board regression (`CONFIG=bare TARGET=test`,
full `wolfcrypt_test` to `Result: 0 (PASS)` on real hardware unless noted).
24 of the attachable targets fully validated (`h5` passes everything except
HW ECDSA-sign; `l552` and `h573` have no board available).

| BOARD  | Chip          | Status        | Notes                                              |
|--------|---------------|---------------|----------------------------------------------------|
| `u5`   | STM32U575ZI   | Validated 6/3 | NUCLEO-U575ZI-Q. HASH + RNG (no AES). Slow SW ECC   |
| `u3`   | STM32U385RG   | Validated 6/3 | NUCLEO-U385RG-Q. TinyAES + HASH + RNG + SAES + PKA. Full DHUK pass + CCB ECDSA (bare + cubemx) |
| `u545` | STM32U545RE   | Validated 6/3 | NUCLEO-U545RE-Q. + DHUK: full pass (SAES + PKA), `DHUK_UNWRAP=1` from non-secure state |
| `u585` | STM32U585AI   | Validated 6/3 | B-U585I-IOT02A. + DHUK: full pass (SAES + PKA), `DHUK_UNWRAP=1` from non-secure state |
| `f207` | STM32F207ZG   | Validated 6/3 | NUCLEO-F207ZG. RNG only (Cortex-M3)                |
| `f303` | STM32F303ZE   | Validated 6/3 | NUCLEO-F303ZE. RNG only. VCP routes (no ext serial needed) |
| `f437` | STM32F437IIHx | Validated 6/3 | STM32439I-EVAL. CRYP + HASH + RNG. UART via ext USB-serial |
| `f439` | STM32F439ZI   | Validated 6/3 | NUCLEO-F439ZI. CRYP + HASH + RNG. 144 MHz PLL      |
| `f767` | STM32F767ZI   | Validated 6/3 | NUCLEO-F767ZI                                      |
| `h7`   | STM32H753ZI   | Validated 6/3 | NUCLEO-H753ZI. CRYP + HASH + RNG. 480 MHz PLL      |
| `h723` | STM32H723ZG   | Validated 6/3 | NUCLEO-H723ZG                                      |
| `h7a3` | STM32H7A3ZI   | Validated 6/3 | NUCLEO-H7A3ZI-Q                                    |
| `h7s3` | STM32H7S3L8   | Validated 6/3 | NUCLEO-H7S3L8. SRAM-load via OpenOCD (no main flash) |
| `g071` | STM32G071RB   | Validated 6/3 | NUCLEO-G071RB. RNG only (Cortex-M0+)               |
| `g474` | STM32G474RE   | Validated 6/3 | NUCLEO-G474RE. TinyAES + RNG. 170 MHz               |
| `g491` | STM32G491RE   | Validated 6/3\* | NUCLEO-G491RE. RNG only. 170 MHz PLL             |
| `l4a6` | STM32L4A6ZG   | Validated 6/3 | NUCLEO-L4A6ZG. TinyAES + HASH + RNG. Slow SW crypto |
| `l562` | STM32L562QE   | Validated 6/3 | STM32L562E-DK                                      |
| `c562` | STM32C562RE   | Validated 6/3 | NUCLEO-C562RE. CRYP + HASH + RNG + SAES + PKA       |
| `c5a3` | STM32C5A3ZG   | Validated 6/3 | NUCLEO-C5A3ZG. SW Hash-DRBG (RNG CONDRST stuck by design) |
| `wba52`| STM32WBA52CG  | Validated 6/3 | NUCLEO-WBA52CG                                     |
| `wl55` | STM32WL55JC   | Validated 6/3 | NUCLEO-WL55JC                                      |
| `wb55` | STM32WB55RG   | Validated     | NUCLEO-WB55RG. TinyAES + RNG, no HASH. (prior session) |
| `n657` | STM32N657X0   | Validated 6/4 | NUCLEO-N657X0-Q. Full HW crypto: SAES-AES (CBC/GCM/CCM) + HASH + RNG + PKA (ECC mul/sign/verify). CPU 600 MHz (PLL1/IC1). SRAM-load |
| `h5`   | STM32H563ZI   | Validated 6/4 | NUCLEO-H563ZI. Full wolfcrypt_test PASS. HW HASH + RNG; ECC in software (H563 PKA ECDSA erratum, see note). FIXED boot fault (VOS0) |
| `c031` | STM32C031C6   | Validated 6/4 | NUCLEO-C031C6. RNG-only (Cortex-M0+). Earlier no-UART was a flaky ST-LINK connection; re-seating the probe resolved it |
| `u083` | STM32U083RC   | Not run 6/3   | NUCLEO-U083RC. RNG only. Not attached this session  |
| `v8`   | STM32V873XJ   | Validated 8/25 | NUCLEO-V873XJ (Cortex-M85, 4 MB). Full wolfcrypt_test PASS + bench PASS; HW RNG + HASH(SHA-1/2) + SAES-AES + PKA ECDSA validated (SHA-3/SHAKE register-level only, wolfSSL integration pending ctx-save procedure); benched at 250/496/800 MHz. Secure-state image at 0x18000000/0x34000000 (no TZEN option byte). CMSIS header is GENERATED from the CubeProgrammer SVD -- see boards/v8/tools/svd2cmsis.py, output not checked in. No UART yet: board_putc writes a .noinit RAM ring buffer read back over SWD. I-cache on, D-cache off (breaks output). SYSCLK 250 MHz measured, CPU_FREQ_BOOST disabled |
| `l552` | STM32L552ZE   | Not run 6/3   | NUCLEO-L552ZE-Q. Not attachable this session        |
| `h573` | STM32H573ZI   | No board      | NUCLEO-H573ZI not available. Build OK               |

\* STM32G491xx silicon has no AES, no HASH, no PKA. The AES + PKA + HASH
blocks are only on the G4A1xx variant in the same G491/G4A1 product line.
BARE on G491RE only accelerates RNG.

DHUK (`TARGET=dhuk`) is fully validated on `u3`, `u545` and `u585` (GMAC +
AES-ECB + ECDSA all routed through SAES/PKA), including `DHUK_UNWRAP=1` from
non-secure state. The `-107` backend gating the two U5 boards once reported
was a missing kernel clock -- SAES on U3/U5 runs from the SHSI, which the bare
driver now enables -- not a TrustZone or provisioning requirement.

CCB (`TARGET=ccb`) is validated on `u3` on both build paths (bare and CubeMX):
a P-256 key is provisioned on-chip with the standard `wc_ecc_make_key` (the crypto callback intercepts it) and signed through
the standard `wc_ecc_sign_hash()`, with the private scalar unwrapped SAES->PKA
in hardware and never entering software. See the "CCB-protected ECDSA" target
section below for details.

`n657` note: N6 AES now routes through the SAES instance
(`WOLFSSL_STM32_USE_SAES`, same TinyAES-shape register path validated on H7S3) --
the N6 "fat" CRYP is in the security domain and times out from NS code, so SAES
is the NS-accessible AES IP. HW AES-CBC/GCM/CCM, HASH and RNG all pass. The CPU
runs at 600 MHz (PLL1 -> IC1); the earlier "PLL may have failed / 64 MHz" banner
was a stale `SystemCoreClockUpdate()` stub (hardcoded 64 MHz) -- it now derives
the real CPUCLK from `RCC->CFGR1` CPUSWS. HW PKA is also enabled and validated:
ECC_MUL (0x20), ECDSA sign (0x24) and verify (0x26) all complete on silicon (the
earlier "ECC_MUL times out" note was stale). N6 BARE now exercises the full HW
crypto block.

`h5` note: this NUCLEO-H563ZI was TrustZone-provisioned (TZEN=1 + SECWM) in a
prior session. `TZEN=0xC3` + mass-erase clears the old "ECC-fault" state and
`FLASH_ECCDETR` stays 0 -- the board is not damaged. The remaining
boot-time fault (HardFault/NMI lockup, stacked PC in newlib `putchar`, faulting
on the first UART output) was root-caused on 2026-06-03 to **missing core
voltage scaling**: `board_init()` ran at 64 MHz HSI with only 1 flash wait
state at the reset-default low VOS, which under-specifies flash and causes
intermittent instruction-fetch bus errors (IBUSERR) once non-trivial code runs.
Raising `PWR->VOSCR` to VOS0 (+wait `VOSRDY`, 2 WS) in `boards/h5/hw_init.c`
fixes it: the board boots and runs the full `wolfcrypt_test`.

H563 PKA ECDSA erratum: HW PKA init + scalar-mul (`make_key`, mode 0x20)
succeed, but **PKA ECDSA *sign*** (mode 0x24) aborts with `OPERRF`
(`SR=0x00200003`, no PROCENDF) with otherwise-correct operands. Confirmed
silicon-level: identical on **two** H563 boards, in the bare-metal driver
**and ST's own `HAL_PKA_ECDSASign`** (which hangs waiting for the PROCENDF
that never comes), and **unchanged in the TZEN=1 secure world** (PKA made
secure via GTZC `PKASEC`, `-mcmse` secure aliases) -- so it is not a
TrustZone/PKA-RAM-access issue. The same code signs fine on N6/U3/U585 (all
V2 PKA). Substituting a software sign then also fails ECDSA *verify*, so the
H563 PKA ECDSA path is unusable. Resolution: `user_settings.h` leaves
`WOLFSSL_STM32_PKA` **off** for `h5` (software ECC; HW HASH + RNG stay on).
See `/tmp/STM32H563_PKA_ECDSA_sign_issue.txt` for the ST report.

## STM32 chip support matrix for `WOLFSSL_STM32_BARE`

The bare-metal driver in `wolfssl` supports a wider matrix of chips than
this example currently wires board files for. To add a new board, drop a
`boards/<name>/` dir with startup, linker, system_*.c, hw_init.c (see the
`h5` and `u5` dirs as templates) and add a BOARD arm to `Makefile` +
`user_settings.h`.

If the new board uses a modern STM32 USART (MODER / AFR / CR1 / BRR /
ISR / TDR shape, i.e. G / H / L / U / N families), `hw_init.c` can call
`board_common_uart_pin_init()` and `board_common_uart_basic_init()` from
`include/board.h` instead of open-coding the GPIO-AF + USART register
setup. The H5 / H7 cluster (h5, h573, h7, h723, h7a3, h7s3) uses these
helpers as a working reference. Each helper inserts a `__DSB()` so the
caller does not need explicit RCC clock-enable read-backs.

### Chips with AES hardware (BARE driver supported)

CRYP IP (FIFO-based, F4/F7/H7 family) -- driver complete in `wolfssl/stm32_bare`:

| Chip                | Board                       | AES IP    | HASH | RNG |
|---------------------|-----------------------------|-----------|------|-----|
| STM32F417           | (custom)                    | CRYP      | yes  | yes |
| STM32F437           | STM32439I-EVAL (G-EVAL)     | CRYP      | yes  | yes |
| STM32F446           | NUCLEO-F446ZE               | CRYP      | yes  | yes |
| STM32F767           | NUCLEO-F767ZI               | CRYP      | yes  | yes |
| STM32H743 / H753    | NUCLEO-H743ZI / H753ZI      | CRYP      | yes  | yes |
| STM32MP135          | STM32MP135F-DK              | CRYP      | yes  | yes |

TinyAES IP (single-register, smaller) -- driver **not yet implemented in
`wolfssl/stm32_bare`** (`#error` until written):

| Chip                | Board                       | AES IP    | HASH | RNG | SAES |
|---------------------|-----------------------------|-----------|------|-----|------|
| STM32H573 / H533    | NUCLEO-H573ZI               | TinyAES   | yes  | yes | yes  |
| STM32U585           | NUCLEO-U585AI-Q             | TinyAES   | yes  | yes | yes  |
| STM32U595/599/U5A5  | (various)                   | TinyAES   | yes  | yes | yes  |
| STM32L475/L476/L496 | NUCLEO-L476RG / L4R5ZI      | TinyAES   | yes* | yes | -    |
| STM32L552 / L562    | NUCLEO-L552ZE-Q             | TinyAES   | yes  | yes | yes  |
| STM32WB55           | NUCLEO-WB55                 | TinyAES   | -    | yes | -    |
| STM32WL55           | NUCLEO-WL55JC               | TinyAES   | -    | yes | -    |
| STM32G031 / G041    | NUCLEO-G031K8 / G041DZ      | TinyAES   | -    | yes | -    |
| STM32G473 / G491    | NUCLEO-G491RE               | TinyAES   | -    | yes | -    |

\* L475 has no HASH; L476/L4R5/L496 do.

### Chips with HASH + RNG only (no AES)

These build under `WOLFSSL_STM32_BARE` with `NO_STM32_CRYPTO`:

| Chip                | Board                       | HASH | RNG |
|---------------------|-----------------------------|------|-----|
| STM32H562 / H563    | NUCLEO-H563ZI               | yes  | yes |
| STM32H523           | NUCLEO-H533RB low-end       | yes  | yes |
| STM32U575 / U535    | NUCLEO-U575ZI-Q             | yes  | yes |
| STM32U545           | (custom)                    | yes  | yes |

### Chips without HASH/AES (RNG-only or pure software baseline)

| Chip                | Board                       | RNG  |
|---------------------|-----------------------------|------|
| STM32H503           | NUCLEO-H503RB               | yes  |
| STM32G0B1 / G0B0    | NUCLEO-G0B1RE               | yes  |
| STM32U383 / U385    | NUCLEO-U385RG-Q             | -    |

For these, build with `CONFIG=c` (pure software) -- `WOLFSSL_STM32_BARE`
has nothing to accelerate.

## Build flavors

| CONFIG | wolfSSL flag(s)                          | wolfCrypt path                                                   |
|--------|------------------------------------------|------------------------------------------------------------------|
| `c`    | (none)                                   | Pure software baseline. No STM32 HW touched, no inline ASM.      |
| `asm`  | `WOLFSSL_ARMASM` + `WOLFSSL_ARMASM_THUMB2` | Cortex-M Thumb2 inline-assembly software impls (no STM32 HW).    |
| `bare` | `WOLFSSL_STM32_BARE`                     | Direct-register STM32 HW (CRYP/AES/HASH/RNG). No HAL/StdPeriph.  |

### Targets (`TARGET=`)

| TARGET | App source        | What it runs                                                                 |
|--------|-------------------|------------------------------------------------------------------------------|
| `test` | `src/main_test.c` | wolfCrypt KATs (SHA-256, AES, RNG) plus the full `wolfcrypt_test` suite.      |
| `bench`| `src/main_bench.c`| The wolfCrypt benchmark suite.                                               |
| `dhuk` | `src/main_dhuk.c` | Transparent DHUK crypto-callback: GMAC, AES-ECB, ECDSA sign, and key-at-rest wrap/unwrap. |
| `mtls` | `src/main_mtls.c` | TLS 1.3 mutual auth, client and server in one image over an in-memory transport. DHUK-wrapped client key where the silicon has it, ordinary key elsewhere. |
| `ccb`  | `src/main_ccb.c`  | Transparent CCB-protected ECDSA (P-256) via `wc_ecc_sign_hash` -- bare + CubeMX. |
| `ccbhal`| `src/main_ccbhal.c`| CubeMX `HAL_CCB_*` reference flow (provision + sign + SW-verify), `u3` only. |
| `cbonly`| `src/main_cbonly.c`| Callback-only: ECDSA, full-payload AES-GCM, HMAC-SHA256, TRNG all on hardware, with the `STM32_BARE_CB_ONLY` software-strip preset. |
| `puf` | `src/main_puf.c` | Configurable SRAM PUF (BCH(127,k,t) fuzzy extractor + HKDF) enroll/reconstruct regression in synthetic-SRAM mode. `PUF_T` selects the BCH profile (7/10/13/15), `PUF_CW` the codeword count. |
| `aesplain`| `src/main_aesplain.c`| Plaintext-key AES vs DHUK-seed AES: registers both devices and selects per `Aes` by devId. Plaintext AES-GCM matches a published KAT; same key bytes yield different ciphertext under each device. SAES+DHUK boards. |
| `plaingcm`| `src/main_plaingcm.c`| Direct `wc_Stm32_Aes_Gcm()` AES-GCM KAT (encrypt, decrypt-verify, tamper-reject) with a plaintext key and NO software fallback -- a pass proves the HW GCM engine produced the output. Boards with AES-GCM silicon only. |
| `cubeaes`| `src/main_cubeaes.c`| `WOLF_CRYPTO_CB_ONLY_AES` on the CubeMX/HAL build: registers the CubeMX AES crypto-callback device and runs AES-GCM KATs (test cases 3 and 4) with a plaintext key. `BUILD=cubemx` + `CONFIG=bare` + `BOARD=u3`. |
| `cubecrypto`| `src/main_cubecrypto.c`| Full HW crypto through the callback on the CubeMX/HAL build under `WOLF_CRYPTO_CB_ONLY_ECC` + `WOLF_CRYPTO_CB_ONLY_AES`: HW ECDSA sign+verify (PKA), CCB-protected ECDSA, and AES-GCM (HAL). `BUILD=cubemx` + `CONFIG=bare` + `BOARD=u3`. |

`TARGET=plaingcm` calls `wc_Stm32_Aes_Gcm()` directly instead of going through `wc_AesGcmEncrypt`, so there is no software GHASH fallback to hide a hardware problem: if it returns 0 and the output matches the published McGrew & Viega test-case-3 vector, the STM32 GCM engine produced it. It needs a board whose silicon actually carries an AES IP with a HW GCM mode -- the CRYP IP on `f437` / `f439` / `h7`, or the TinyAES IP (routed to SAES on `h7s3` / `n657`) on `h7s3`, `u3`, `u585`, `u545`, `l4a6`, `l562`, `wba52`, `n657`. Boards with no AES silicon (`u5`, `f767`, `l552`) and the C5 boards are rejected by the Makefile. `c5a3` / `c562` have the IP, but the STM32C5 CMSIS names the GCM phase field `AES_CR_CPHASE` rather than `AES_CR_GCMPH`, and wolfSSL only abstracts that rename on the DHUK/SAES path -- the plaintext-key `wc_Stm32_Aes_Gcm()` compiles to the `CRYPTOCB_UNAVAILABLE` stub there, so use `TARGET=aesplain` on C5 instead.

```
make BOARD=f439 CONFIG=bare TARGET=plaingcm flash
make BOARD=u3   CONFIG=bare TARGET=plaingcm flash
```

`TARGET=cubeaes` and `TARGET=cubecrypto` are the CubeMX/HAL counterparts to `aesplain` and `cbonly`: both require `BUILD=cubemx` (ST's HAL drivers rather than the direct-register path), `CONFIG=bare` (which is what defines `WOLFSSL_STM32_CUBEMX`) and `BOARD=u3` (NUCLEO-U385RG-Q, the board this tree carries a CubeMX HAL init for). `cubeaes` proves `WOLF_CRYPTO_CB_ONLY_AES` works over the HAL: the device's AES-ECB handler lets `wc_AesGcmSetKey` derive the GHASH subkey H on hardware, after which bulk GCM runs on the native HAL engine. `cubecrypto` is the wider config shape -- software ECC and AES both stripped, so ECDSA sign/verify go to the PKA, CCB-protected ECDSA to `HAL_CCB_*`, and AES-GCM to the HAL. The application brings up the ST HAL PKA itself (`HAL_PKA_Init` on the `hpka` handle the board file defines). Its CCB step reports `SKIPPED` rather than PASS, because on-chip provisioning needs the software keygen that `WOLF_CRYPTO_CB_ONLY_ECC` removes; provision the blob in a non-stripped build to exercise the CCB sign path.

```
make BOARD=u3 BUILD=cubemx CONFIG=bare TARGET=cubeaes    flash
make BOARD=u3 BUILD=cubemx CONFIG=bare TARGET=cubecrypto flash
```

`TARGET=aesplain` registers the plaintext-key AES device (`wc_Stm32_AesRegister(WOLFSSL_STM32_AES_DEVID)`) alongside the DHUK device (`wc_Stm32_DhukRegister(WC_DHUK_DEVID)`) in one bare `STM32_BARE_CB_ONLY` build, then proves an `Aes` on the plaintext devId reproduces a published AES-GCM vector (key used verbatim) while an `Aes` on the DHUK devId turns the same bytes into a device-bound key -- the two ciphertexts differ and each device decrypts its own output. Limited to the SAES + DHUK boards (`u3`, `u585`, `u545`, `c5a3`, `c562`).

`TARGET=dhuk` is limited to the SAES + PKA + DHUK boards (`u3`, `u585`, `u545`) and adds `-DWOLFSSL_DHUK -DWOLF_CRYPTO_CB`, which enable the STM32 DHUK crypto-callback device (in `wolfcrypt/src/port/st/stm32.c`). An application registers the device once (`wc_Stm32_DhukRegister(WC_DHUK_DEVID)`), inits a normal `Aes` / `ecc_key` with `devId = WC_DHUK_DEVID`, supplies the 256-bit seed as the key (`wc_AesGcmSetKey` / `wc_AesSetKey`) or via `wc_ecc_import_wrapped_private`, then performs NORMAL wolfCrypt calls -- the device-bound key is derived inside SAES and never appears in software. `main_dhuk.c`:

- runs `wc_ecc_import_wrapped_private` input-validation as a hard PASS/FAIL software unit test (no SAES/PKA needed), and
- exercises GMAC, AES-ECB, and ECDSA sign transparently through the crypto-callback path on real silicon.

The DHUK AES-GCM path handles full (nonzero) payloads, not just GMAC: the SAES runs H, E(J0) and the CTR keystream under the device-derived key while GHASH runs in software, so `wc_AesGcmEncrypt` / `wc_AesGcmDecrypt` on a `WC_DHUK_DEVID` `Aes` produce a device-bound AEAD whose key never enters software (exercised by `TARGET=cbonly` below).

The optional exact-key import primitive (`wc_Stm32_Aes_DhukOp`, imports an externally-chosen key rather than deriving from a seed) stays gated behind `-DWOLFSSL_STM32_DHUK_UNWRAP` and is off by default.

Two different protections are available, and which one applies depends on whether the application needs the key *bytes*:

- **Key stays in hardware.** A chosen AES-256 key is wrapped with `wc_Stm32_Aes_Wrap_ex()` and the blob handed straight back to `wc_AesSetKey()` on a `WC_DHUK_DEVID` `Aes`, so SAES loads it into `KEYR`. The key never reaches RAM, but only the AES engine can use it. Flow `[B]` of test `[8]`.
- **Key unwrapped into RAM.** The DHUK-derived key acts as a KEK: the payload is encrypted under it at provisioning and decrypted back into a caller-owned buffer at runtime. The KEK itself still never enters software -- only the payload does, and only while it is in use. This is the generic route for key material the AES engine cannot hold: a 128-bit key, an ECC scalar (flow `[C]`), or a key belonging to an algorithm outside wolfCrypt. Flow `[B2]` of test `[8]`, and test `[9]`. For an ECC scalar specifically, parts with a CCB (`u3`) can keep the scalar out of software altogether by unwrapping it SAES->PKA in hardware; see "CCB-protected ECDSA" below.

Test `[12]` is the second pattern in the form a product usually needs it. It uses AES-GCM rather than raw ECB, which buys two things: the payload length is arbitrary (ECB and CBC both require a multiple of the block size, so a key that is not AES-shaped would otherwise need a padding convention), and the tag rejects a corrupted blob, or one produced under another seed or stored under another key id, instead of returning garbage key bytes. It wraps a deliberately awkward 37-byte key and proves both properties, the second by corrupting a blob, by decrypting under the wrong seed and by presenting the blob under the wrong key id -- each must fail with `AES_GCM_AUTH_E`. What the tag does not cover is rollback: an older valid `{iv, blob, tag}` for the same key id authenticates just as well, because nothing in the record carries a generation number. A product that needs rollback protection binds a trusted monotonic counter into the AAD and rejects records older than its stored value.

Test `[12]` generates a fresh IV per payload from the TRNG and stores it beside the blob. That is not incidental: AES-GCM needs a unique nonce for every payload encrypted under one key, and reusing one across two payloads under the same seed forfeits both confidentiality and the tag. The IV is not secret, so it can sit in flash next to the blob.

`wc_Stm32_Aes_DhukOp_ex()` is the exact-key form of the KEK pattern: rather than deriving from a seed, it unwraps a blob inside `KEYR` and ciphers with the key that blob wrapped. Provisioning is a separate step -- wrap the chosen key `K` once with `wc_Stm32_Aes_Wrap_ex()` in `WC_STM32_WRAP_ORDER_RAW` order and store the 32-byte blob; at runtime stage that blob, not `K`, in `aes->key` and call `wc_Stm32_Aes_DhukOp_ex()`. Test `[6]` asserts the identity, failing unless `DhukOp ct == AES-ECB(K as-is, pt)`; test `[13]` does the same over a four-block payload through ECB and CBC, comparing every block against software AES-256 keyed with `K`. Both need `DHUK_UNWRAP=1` and are verified on `u3`, `u585` and `u545` from non-secure state. The path requires block-multiple payloads and carries no integrity tag -- `[13]` demonstrates that a corrupted blob decrypts to wrong bytes with no error -- so test `[12]`'s AEAD route is the better default.

```
make BOARD=u3 CONFIG=bare TARGET=dhuk flash
```

Hardware test results (`u3` / NUCLEO-U385RG-Q, CONFIG=bare, wolfSSL 5.9.1, captured 2026-06-01):

```
========================================
wolfCrypt DHUK test - NUCLEO-U385RG-Q (CONFIG=bare)
wolfSSL version: 5.9.1
SYSCLK: expected 16000000 Hz, CMSIS-reported 16000000 Hz (match)
========================================

[1] ECC DHUK setter validation (SW unit test):
  accept P-256 (32/32): 0 OK
  accept P-521 (80/66): 0 OK
  reject wrappedLen=20: -173 OK
  reject wrappedLen=0: -173 OK
  reject wrappedLen=112: -173 OK
  reject plain=48 > wrapped=32: -173 OK
  reject wrapped=48 > roundup16(plain=16): -173 OK
  setter validation OK

[2] GMAC via transparent DHUK crypto-callback:
  cryptocb GMAC tag: c7 65 e9 1a ea 00 9b ea c5 36 76 b1 e4 44 1a 85
  cryptocb GMAC deterministic OK
  cryptocb GMAC verify OK (round-trip)

[3] AES-ECB via transparent DHUK crypto-callback:
  cryptocb ECB ct: 26 01 cf 4e d5 e3 8d b4 36 8b e0 00 8f 7a f4 15
  cryptocb ECB round-trip OK
  cryptocb ECB seed-dependent OK (DHUK key drives cipher)

[4] ECDSA sign via transparent DHUK crypto-callback:
  plain PKA sign+verify: rc=0 verify=1
  DHUK ECDSA sign produced a 70-byte signature
  DHUK ECDSA verify OK (signed via DHUK, verified with pubkey)

Result: 0 (PASS)
Test complete
```

`-173` is `BAD_FUNC_ARG` (the setter rejects every malformed length, including a wrapped blob larger than the plaintext padded to a full AES block). Tests [2]-[4] run the real SAES + PKA hardware: the GMAC tag is deterministic and verifies, AES-ECB round-trips and is seed-dependent (proving the DHUK-derived key drives the cipher), and the ECDSA signature verifies against its public counterpart. The `u585` and `u545` builds link cleanly but were not run on hardware here: the only U585 board on the bench is a B-U585I-IOT02A, whose ST-LINK VCP is not wired to the USART1 PA9/PA10 pins this example's `u585` build drives (it targets the NUCLEO-U585AI-Q), and no `u545` board is attached.

#### TZEN=1 secure-state note (B-U585I-IOT02A)

The crypto-callback path above runs on STM32U385 with TZEN=0. On the B-U585I-IOT02A (which ships `TZEN=0x1` with bank-1 secure-watermarked, so the image at `0x08000000` runs secure) a `SECURE=1` build (`-mcmse`, which makes the CMSIS device header resolve every peripheral to its secure alias) was used to reach secure state. One board fix was needed: `board_init` had an unbounded wait on `PWR_SVMSR.REGS` after requesting the LDO->SMPS switch (`PWR_CR3.REGSEL`), which never latches on this board; that loop is now bounded and falls through on the already-ready LDO. Results are read from the `g_dhuk_res` debugger sink (the IOT02A VCP is not wired to this build's USART1 pins): the firmware reaches secure state and the setter validation passes, but the SAES key derivation currently stalls (`SR.BUSY` does not clear) under TZEN=1 secure context. **Secure execution alone does not unblock the derive on this silicon** -- it likely needs explicit GTZC/TZSC SAES-secure (and SAES RNG) configuration; this is open work. DHUK does not otherwise require secure state.

#### mTLS with a DHUK-wrapped client key (`TARGET=mtls`)

`TARGET=mtls` runs a TLS 1.3 client and server in one image over a pair of
in-memory buffers, so there is no network stack, no sockets and no host involved.
It is the only TLS target in this harness; everything else here is wolfCrypt only.

On a board with SAES + DHUK + PKA (`u3`, `u585`, `u545`) the client's private key
exists only as a DHUK-wrapped blob, which answers the narrow question: can a TLS
client authenticate with a key that never appears in software? Those three boards
require `CONFIG=bare`, the config that enables the PKA. On every other
board the same handshake runs with an ordinary in-memory client key, so the target
doubles as a plain TLS 1.3 mutual-auth regression test. `MTLS_HAVE_DHUK` selects
between the two, and the board set is limited only by memory -- TLS 1.3 plus ECC
and the cert buffers needs roughly 250 KB of flash and 165 KB of RAM.

The piece that makes it work is a second crypto-callback device.
`wc_ecc_import_wrapped_private()` puts the blob on an `ecc_key` the application
owns, but TLS builds its own key internally -- from a DER buffer, or for an opaque
key from a key id -- and that key carries no blob, so the DHUK device declines it
and the handshake falls back to the software signer. Handing TLS a key *id* bound
to a second device (`wolfSSL_CTX_use_PrivateKey_Id`) routes the CertificateVerify
signature to the provisioned key instead. The certificate must be loaded before the
key id: loading it is what sets the CTX private key type and size, which the id path
does not carry. The devId belongs on the key, not on the CTX -- a CTX-wide devId
would also send the ephemeral ECDHE keygen to the device.

Test `[3]` is a negative control, and only runs where there is a device: it
unregisters it and re-runs the same handshake, which must fail with `-170`.
Without it, a passing `[2]` would not show that the wrapped key was what
authenticated the client.

```bash
make BOARD=u585 CONFIG=bare TARGET=mtls flash
```

Nothing in `main_mtls.c` is family-specific and it does not use the U3 CCB.

| Board | Family | Client key | Result |
|---|---|---|---|
| `u3` (NUCLEO-U385RG-Q)  | U3 | DHUK-wrapped | PASS |
| `u585` (B-U585I-IOT02A) | U5 | DHUK-wrapped | PASS |
| `u545` (NUCLEO-U545RE-Q)| U5 | DHUK-wrapped | PASS |
| `h5` (NUCLEO-H563ZI)    | H5 | ordinary     | PASS |
| `f767` (NUCLEO-F767ZI)  | F7 | ordinary     | PASS |
| `l562` (STM32L562E-DK)  | L5 | ordinary     | PASS |
| `f439` (NUCLEO-F439ZI)  | F4 | ordinary     | builds; not run on hardware |

#### CCB-protected ECDSA (`TARGET=ccb`)

`TARGET=ccb` is `u3`-only (the CCB peripheral is STM32U3 silicon) and adds `-DWOLFSSL_DHUK -DWOLF_CRYPTO_CB -DWOLFSSL_STM32_CCB`. The CCB (Coupling and Chaining Bridge, RM0487 ch 31) chains the PKA, SAES and RNG in hardware so a DHUK-protected ECDSA private scalar is unwrapped by the SAES and consumed by the PKA without ever crossing the system bus or entering software -- a stronger guarantee than `TARGET=dhuk`, where the generic DHUK ECDSA path decrypts the scalar into a short-lived stack buffer. `main_ccb.c` provisions a device-bound P-256 key on-chip with the standard `wc_ecc_make_key()` -- the STM32 crypto callback intercepts keygen and binds a CCB-protected blob (no CCB-specific API) -- then signs through `wc_ecc_sign_hash()` (also routed to the CCB) and verifies with the derived public key.

It builds on **both** paths -- bare-metal (direct-register OPSTEP driver) and CubeMX (ST `HAL_CCB_*`):

```
make BOARD=u3 CONFIG=bare  TARGET=ccb flash
make BOARD=u3 BUILD=cubemx TARGET=ccb flash
```

A separate `TARGET=ccbhal` (`main_ccbhal.c`) drives ST's `HAL_CCB_*` API directly (provision + sign + software verify) as a known-good HAL reference; it is `u3` / CubeMX only.

Hardware test results (`u3` / NUCLEO-U385RG-Q, captured 2026-06-11; bare shown, CubeMX identical):

```
========================================
wolfCrypt CCB ECDSA test - NUCLEO-U385RG-Q (CONFIG=bare)
========================================

[1] wc_ecc_make_key (provision P-256 CCB key on-device):
    ret = 0
[2] wc_ecc_sign_hash (transparent crypto-callback):
    ret = 0 (sigLen=70)
[3] wc_ecc_verify_hash (vs CCB-derived public key):
    ret = 0 verified = 1

Result: 0 (PASS)
```

Both build paths pass on TZEN=0; the P-256 scalar never appears in software. The CCB needs the U3 at its full clock -- `boards/u3/hw_init.c` (bare) and `hw_init_cubemx.c` (CubeMX) bring the U385 up to 96 MHz (MSIS + EPOD booster + VOS range 1 + 2 wait-states, LDO supply).

#### Callback-only build (`TARGET=cbonly`)

`TARGET=cbonly` runs the four primitives an embedded TLS / secure-boot stack needs -- ECDSA (sign + verify), full-payload AES-GCM, HMAC-SHA256 and TRNG -- entirely on hardware through the crypto-callback framework, and turns on the `STM32_BARE_CB_ONLY` preset that strips the software crypto a callback-only build does not need (RSA, DH, the large SP moduli, SHA-384/512, Curve25519/Ed25519, ChaCha/Poly1305, AES-CCM, PBKDF, cert/key gen and the error-string table). ECDSA runs on the PKA, AES-GCM on the SAES (CTR + GHASH), HMAC-SHA256 on the STM32 HASH block (the callback declines HMAC so `hmac.c`'s hardware `innerHashKeyed` path runs), and the TRNG through a new `WC_ALGO_TYPE_RNG` callback case backed by `wc_GenerateSeed`. Limited to the SAES + PKA + DHUK boards (`u3`, `u585`, `u545`, `c5a3`, `c562`); adds `-DWOLFSSL_DHUK -DWOLF_CRYPTO_CB -DSTM32_BARE_CB_ONLY`.

```
make BOARD=u3 CONFIG=bare TARGET=cbonly flash
```

Hardware test results (`u3` / NUCLEO-U385RG-Q, captured 2026-07-02):

```
[1] ECDSA sign(DHUK) + verify:
  ECDSA sign(DHUK)+verify OK (71-byte sig)

[2] AES-GCM full payload (DHUK, SAES CTR + GHASH):
  GCM round-trip OK (plaintext recovered, tag verified)
  GCM tamper rejected (AES_GCM_AUTH_E) OK

[3] HMAC-SHA256 (DHUK devId -> STM32 HASH block):
  HMAC-SHA256 matches RFC 4231 case 1 OK

[4] TRNG via crypto-callback (DHUK devId):
  TRNG produced 32 bytes of entropy OK

Result: 0 (PASS)
```

The AES-GCM leg validates enc/dec with a nonzero payload round-trip plus tamper rejection -- the right correctness bar for a device-bound key, whose value is never known so a fixed KAT cannot apply. Dropping the software crypto shrinks flash measurably: `parse_size.py` reports `cbonly` vs the full-software `dhuk` build at 9.7 KB saved on `u3` (12.3%), 11.8 KB on `u585` (15.0%), and 15.0 KB on `c5a3` (17.0%).

### SRAM PUF regression (`TARGET=puf`)

`TARGET=puf` builds `src/main_puf.c`, exercising the configurable wolfCrypt SRAM PUF (a BCH(127,k,t) fuzzy extractor with HKDF key derivation) end to end on real silicon in synthetic-SRAM mode (`WOLFSSL_PUF_TEST`), so it runs on any board with no board-specific NOLOAD section. Each run enrolls, reconstructs cleanly (identity and derived key must match), reconstructs at the `t`-bit correction limit (`t` flips per 128-bit block, must still match), and checks an over-limit `t+1`-flip case that must fail or produce a different identity rather than silently reproduce the enrolled key. It prints per-step PASS lines then `Result: 0 (PASS)` and `Test complete`. For the real power-on SRAM physical PUF, use the H5-specific `wolfssl-examples/puf` project instead.

This target uses `wc_PufGetProfileId()`, `wc_PufGetHelperData()` and `wc_PufReconstructEx()`, so it needs a wolfSSL with the configurable-PUF work; it will not build against 5.9.2. Step `[0]` cross-checks the library's profile id against the application's, which is what catches a partial rebuild that mixes objects from two different profiles (the sizes of `wc_PufCtx` disagree and the context overruns memory), and step `[5]` confirms helper data from a foreign profile is refused rather than decoded into a silently wrong key.

Two knobs select the variation, gated via `-DSTM32_BARE_PUF` (which enables `WOLFSSL_PUF` + `WOLFSSL_PUF_TEST` in `user_settings.h`; HKDF is already on): `PUF_T=<7|10|13|15>` picks the BCH profile (`-DWC_PUF_BCH_T`, default 10) and `PUF_CW=<n>` picks `WC_PUF_NUM_CODEWORDS` (default 16). Because `make` does not track CFLAGS changes, give each profile its own `BUILD_DIR` (or `make clean` between profiles) or the build silently reuses stale wrong-profile objects.

```
make BOARD=h5   CONFIG=bare TARGET=puf            flash   # default t=10
make BOARD=h5   CONFIG=bare TARGET=puf PUF_T=13   flash
make BOARD=f767 CONFIG=bare TARGET=puf PUF_T=7    flash
```

Validated on real silicon:

| Board | Core | Variation | Result |
|-------|------|-----------|--------|
| NUCLEO-H563ZI | Cortex-M33 | t=7 / 10 / 13 / 15 | PASS |
| NUCLEO-H563ZI | Cortex-M33 | t=10, cw=8 / cw=32 | PASS |
| NUCLEO-G071RB | Cortex-M0+ | t=10 | PASS |
| NUCLEO-F439ZI | Cortex-M4F | t=10, t=13 | PASS |
| NUCLEO-F767ZI | Cortex-M7 | t=10, t=7 | PASS |
| B-U585I-IOT02A | Cortex-M33 (TrustZone) | t=10 | PASS |

### Orthogonal axes

| Axis     | Default | Values         | Effect                                                              |
|----------|---------|----------------|---------------------------------------------------------------------|
| `BUILD`  | `bare`  | `bare`,`cubemx`| `cubemx` swaps `hw_init.c` for `hw_init_cubemx.c` and pulls in the ST HAL driver pack instead of direct-register init. Wolfcrypt port flag becomes `WOLFSSL_STM32_CUBEMX`. |
| `STACK`  | `0`     | `0`,`1`        | `STACK=1` compiles in wolfssl's memory/stack trackers (`-DSTM32_BARE_STACK_TRACK`) so `TARGET=bench` reports per-algo heap/stack and a cumulative peak. `bench_matrix.sh -S` runs it as a companion pass; see "Stack and heap measurements" below. |
| `PQC`    | `0`     | `0`,`1`        | `PQC=1` enables ML-DSA (Dilithium) and ML-KEM with the small-memory variants (`WOLFSSL_DILITHIUM_NO_LARGE_CODE`, `WOLFSSL_DILITHIUM_SMALL`, `WOLFSSL_DILITHIUM_VERIFY_SMALL_MEM`, `WOLFSSL_DILITHIUM_VERIFY_NO_MALLOC`, `WOLFSSL_MLKEM_MAKEKEY_SMALL_MEM`, `WOLFSSL_MLKEM_ENCAPSULATE_SMALL_MEM`) plus the SHA-3 / SHAKE-128/256 dependency. Adds ~100 KB of text -- small-flash boards (c031, g071, u083) may overflow. Mirrors the wolfBoot resource-constrained preset. |

## Build matrix (TARGET=test, all 27 boards)

All board / config combinations are kept building so a regression in either the BARE driver or the SP-math + ARMASM path on any of the supported chips shows up on the first `make`. Numbers below are from one full pass of `make BOARD=<x> CONFIG=<y> TARGET=test PQC=0` (refreshed 2026-05-18 after recent wolfssl master growth in the asm / sp-math paths).

### Build size matrix

Sizes from `arm-none-eabi-size` Berkeley format (text + data is what lands in flash; bss is RAM consumed at runtime). "N/A" cells either fail to build by design (M0/M0+ has no Thumb2 ASM, so `CONFIG=asm` cannot link on c031/g071/u083) or overflow flash (C031 32 KB part holds only the SHA-256+AES smoke surface in `main_test.c`; the wolfcrypt_test suite is omitted on that board. WL55's 256 KB flash now only fits `CONFIG=bare`).

| Board (BOARD=) | Chip | Core | MHz | Flash | RAM | bare text+data | asm text+data | c text+data | bare flash% |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| `c031` | STM32C031C6 | M0+ | 12 | 32 KB | 12 KB | 15.5 KB | N/A | 15.5 KB | 48% |
| `c5a3` | STM32C5A3ZG | M33F | 144 | 1024 KB | 256 KB | 256.1 KB | 379.4 KB | 275.5 KB | 25% |
| `f207` | STM32F207ZG | M3 | 120 | 1024 KB | 128 KB | 277.8 KB | 385.1 KB | 277.5 KB | 27% |
| `f303` | STM32F303ZE | M4F | 64 | 512 KB | 64 KB | 276.6 KB | 380.5 KB | 276.6 KB | 54% |
| `f437` | STM32F437IIH | M4F | 144 | 2048 KB | 192 KB | 254.3 KB | 379.3 KB | 275.2 KB | 12% |
| `f439` | STM32F439ZI | M4F | 144 | 2048 KB | 192 KB | 254.3 KB | 379.3 KB | 275.2 KB | 12% |
| `f767` | STM32F767ZI | M7F | 216 | 2048 KB | 320 KB | 277.0 KB | 382.5 KB | 276.7 KB | 14% |
| `g071` | STM32G071RB | M0+ | 16 | 128 KB | 36 KB | 124.4 KB | N/A | 124.4 KB | 97% |
| `g491` | STM32G491RE | M4F | 170 | 512 KB | 96 KB | 277.0 KB | 380.6 KB | 276.8 KB | 54% |
| `h5` | STM32H563ZI | M33F | 64 | 2048 KB | 640 KB | 257.7 KB | 380.5 KB | 276.9 KB | 13% |
| `h573` | STM32H573ZI | M33F | 64 | 2048 KB | 640 KB | 240.2 KB | 379.3 KB | 275.4 KB | 12% |
| `h7` | STM32H753ZI | M7F | 480 | 2048 KB | 512 KB | 253.7 KB | 412.9 KB | 274.4 KB | 12% |
| `h723` | STM32H723ZG | M7F | 64 | 1024 KB | 320 KB | 277.3 KB | 382.8 KB | 277.0 KB | 27% |
| `h7a3` | STM32H7A3ZI-Q | M7F | 280 | 2048 KB | 256 KB | 275.5 KB | 381.0 KB | 275.2 KB | 13% |
| `h7s3` | STM32H7S3L8 | M7F | 64 | 256 KB | 64 KB | 238.3 KB | N/A | N/A | 93% |
| `l4a6` | STM32L4A6ZG | M4F | 16 | 1024 KB | 192 KB | 253.5 KB | 379.3 KB | 275.3 KB | 25% |
| `l552` | STM32L552ZE-Q | M33F | 16 | 512 KB | 256 KB | 270.8 KB | 380.2 KB | 276.6 KB | 53% |
| `l562` | STM32L562E-DK | M33F | 16 | 512 KB | 256 KB | 246.7 KB | 378.8 KB | 275.0 KB | 48% |
| `n657` | STM32N657X0-Q | M55F | 600 | 2048 KB | 1536 KB | 266.9 KB | 380.6 KB | 280.6 KB | 13% |
| `u083` | STM32U083RC | M0+ | 16 | 256 KB | 40 KB | 157.6 KB | N/A | 175.3 KB | 62% |
| `u3` | STM32U385RG | M33F | 16 | 1024 KB | 256 KB | 239.8 KB | 378.9 KB | 275.0 KB | 23% |
| `u5` | STM32U575ZI-Q | M33F | 16 | 2048 KB | 768 KB | 271.1 KB | 380.4 KB | 276.8 KB | 13% |
| `u545` | STM32U545RE-Q | M33F | 96 | 512 KB | 256 KB | 247.6 KB | 379.2 KB | 275.4 KB | 48% |
| `u585` | STM32U585AI | M33F | 96 | 2048 KB | 768 KB | 247.7 KB | 379.4 KB | 275.5 KB | 12% |
| `wb55` | STM32WB55RG | M4F | 64 | 1024 KB | 192 KB | 253.0 KB | 379.4 KB | 275.3 KB | 25% |
| `wba52` | STM32WBA52CG | M33F | 100 | 1024 KB | 128 KB | 247.2 KB | 378.9 KB | 275.0 KB | 24% |
| `wl55` | STM32WL55JC | M4F | 16 | 256 KB | 64 KB | 253.8 KB | N/A | N/A | 99% |

Observations:

- `bare` is consistently the smallest text size on any chip that has HW accelerators (no SP-math software code for AES/SHA/RSA/ECC, those go directly to the IP block).
- `asm` is the largest text size on every chip because it pulls in SP-math 2048/3072/4096 RSA, ECC P-256/P-384/P-521, and Dilithium (M44/M65/M87) in hand-scheduled Thumb2 assembly.
- `c` sits between `bare` and `asm`: SP-math is included as C source rather than ASM, but the AES/SHA software impls are linked too.
- The `bss` numbers (not shown above; see CSV in `/tmp/build_matrix.csv`) are dominated by the benchmark.c global key/data buffers and are essentially identical across configs on a given board.

### PQC=1 size delta (CONFIG=bare, TARGET=test)

`PQC=1` enables ML-DSA (Dilithium) + ML-KEM with the small-memory variants (see "Orthogonal axes" above). The text-size delta vs `PQC=0` is roughly +100 KB on every board. Captured with `make BOARD=<x> CONFIG=bare TARGET=test PQC=1`.

| BOARD   | Chip          | Flash  | PQC=1 text | PQC=1 bss  | Fits? |
|---------|---------------|-------:|-----------:|-----------:|------|
| `h7`    | STM32H753ZI   | 2048 KB| 354.2 KB   | 129.5 KB   | yes  |
| `u585`  | STM32U585AI   | 2048 KB| 348.2 KB   | 129.5 KB   | yes  |
| `u5`    | STM32U575ZI-Q | 2048 KB| 371.5 KB   | 129.5 KB   | yes  |
| `h5`    | STM32H563ZI   | 2048 KB| 358.0 KB   | 97.5 KB    | yes  |
| `h573`  | STM32H573ZI   | 2048 KB| 340.6 KB   | 97.5 KB    | yes  |
| `f439`  | STM32F439ZI   | 2048 KB| 354.5 KB   | 145.5 KB   | yes  |
| `n657`  | STM32N657X0-Q | 2048 KB| 367.7 KB   | 129.5 KB   | yes  |
| `h7a3`  | STM32H7A3ZI-Q | 2048 KB| 376.0 KB   | 129.5 KB   | yes  |
| `g491`  | STM32G491RE   | 512 KB | 377.2 KB   | 33.5 KB    | yes  |
| `l552`  | STM32L552ZE-Q | 512 KB | 371.2 KB   | 33.5 KB    | yes  |
| `u545`  | STM32U545RE-Q | 512 KB | 348.1 KB   | 129.5 KB   | yes  |
| `wba52` | STM32WBA52CG  | 1024 KB| 347.7 KB   | 33.5 KB    | yes  |
| `wb55`  | STM32WB55RG   | 1024 KB| 353.2 KB   | 65.5 KB    | yes  |
| `h7s3`  | STM32H7S3L8   | 256 KB | -          | -          | overflow (256 KB flash) |
| `u083`  | STM32U083RC   | 256 KB | -          | -          | overflow (256 KB flash) |
| `g071`  | STM32G071RB   | 128 KB | -          | -          | overflow + needs ASN  |
| `c031`  | STM32C031C6   | 32 KB  | -          | -          | overflow              |
| `c5a3`  | STM32C5A3ZG   | 1024 KB| -          | -          | needs CMSIS system file |

Silicon-validated PQC=1 on the H753: full wolfcrypt_test passes including the MLKEM and DILITHIUM self-tests.

PQC=1 cross-product on H753 (text-size):

| CONFIG  | BUILD  | text     | notes                                                       |
|---------|--------|---------:|-------------------------------------------------------------|
| `bare`  | bare   | 354.2 KB | reference (table above)                                     |
| `bare`  | cubemx | 365.3 KB | +~11 KB HAL drivers                                         |
| `c`     | bare   | 374.9 KB | software baseline + PQC                                     |
| `asm`   | bare   | 378.8 KB | wolfssl thumb2-mlkem-asm_c.c is conditionally added by the Makefile when PQC=1. SHA3 stays on the C path (thumb2-sha3 ASM is available upstream but would push wl55 over its 256 KB flash) -- the asm config sets `WC_SHA3_NO_ASM` to scope WOLFSSL_ARMASM off for sha3.c only. |

`g071` -- PQC=1 builds need wolfssl ASN enabled (`oidHashType` is pulled by dilithium.c). Trimmed-surface boards that disable ASN need their user_settings.h to re-enable ASN before PQC=1 will compile.

### Hardware accelerator coverage per board

Marks which HW IP blocks each chip exposes that `WOLFSSL_STM32_BARE` drives. "-" means the silicon does not have the IP; the SW fallback runs in `bare` for that algorithm. PKA column distinguishes V1 (WB/WL/L5) from V2 (U5/U3/H5/WBA/H7S/N6/C5) -- the two register layouts are handled by separate code paths in `wolfcrypt/src/port/st/stm32.c`.

| Board | AES | HASH | RNG | PKA | SAES | DHUK | Status |
|---|---|---|---|---|---|---|---|
| `c031` | - | - | - | - | - | - | test PASS; bench overflows 32 KB flash |
| `c5a3` | TinyAES | HW | HW* | V2 | HW | HW | test PASS (SW RNG); HW RNG init stuck, ST escalation pending |
| `f207` | - | - | HW | - | - | - | test PASS |
| `f303` | - | - | - | - | - | - | test PASS |
| `f437` | CRYP | HW | HW | - | - | - | test PASS |
| `f439` | CRYP | HW | HW | - | - | - | bench validated |
| `f767` | CRYP | HW | HW | - | - | - | test PASS |
| `g071` | - | - | - | - | - | - | test PASS |
| `g491` | - | - | HW | - | - | - | bench validated |
| `h5` | - | HW | HW | - | - | - | test PASS |
| `h573` | TinyAES | HW | HW | - | HW | HW | silicon validation pending TZEN clear |
| `h7` | CRYP | HW | HW | - | - | - | bench validated |
| `h723` | - | - | HW | - | - | - | test PASS |
| `h7a3` | - | - | HW | - | - | - | test PASS at PLL 280 MHz (HSI / M=4 / N=35 / P=2; VOS Scale 0; FLASH 6 WS) |
| `h7s3` | SAES | HW | HW | V2 | HW | - | test PASS |
| `l4a6` | TinyAES | HW | HW | - | - | - | test PASS |
| `l552` | - | HW | HW | V1 | - | - | test PASS |
| `l562` | TinyAES | HW | HW | V1 | HW | - | test PASS |
| `n657` | TinyAES | HW | HW | V2 | HW | HW | test PASS |
| `u083` | TinyAES | - | HW | - | - | - | test PASS |
| `u3` | TinyAES | HW | HW | V2 | HW | HW | test PASS |
| `u5` | - | HW | HW | V2 | - | HW | test PASS |
| `u545` | - | HW | HW | V2 | - | HW | test PASS |
| `u585` | TinyAES | HW | HW | V2 | HW | HW | test PASS |
| `wb55` | TinyAES | - | HW | V1 | - | - | bench validated |
| `wba52` | TinyAES | HW | HW | V2 | HW | HW | test PASS |
| `wl55` | TinyAES | - | HW | V1 | - | - | test PASS |

\* C5A3 HW RNG NIST init currently stuck on cold-boot (CONDRST=1, BUSY=1 do not clear); SW DRBG is used. Open issue with STMicroelectronics.

**H7A3 PLL bring-up (resolved 2026-05-14):** running at 280 MHz via PLL1 from HSI 64 MHz with M=4 (16 MHz ref) / N=35 (560 MHz VCO) / P=2, VOS Scale 0, FLASH 6 WS, SMPS-direct supply. Key insight from the investigation: the H7A3 boots in "Run* mode" with `PWR_CR3 = 0x06` (LDOEN+SMPSEN both pending) and `ACTVOSRDY = 0`. The supply commit MUST be done at runtime (not via `USE_PWR_*_SUPPLY` startup defines). Doing the commit from startup-time `ExitRun0Mode()` -- which runs before SystemInit, before UART is up, before SWD has fully attached -- and having it hang leaves the chip in a state where SWD itself stops responding; only a USB power-cycle (or BOOT0=VDD bootloader entry) recovers. Doing the same write at runtime with a bounded `wait_bit_set()` falls back cleanly to HSI 64 MHz if the supply choice doesn't commit, keeping SWD up. The runtime supply-commit code lives in `boards/h7a3/hw_init.c::clock_init_pll280()` and mirrors `HAL_PWREx_ConfigSupply()`. Also relevant: PLL must source from HSI (not HSE) since NUCLEO-H7A3ZI-Q does not bridge ST-LINK MCO to OSC_IN by default. Reference: STM32CubeIDE-generated `SystemClock_Config` on this exact board.

## Benchmarks

Captured by flashing `BOARD=<x> CONFIG=<c|asm|bare> TARGET=bench` and copying
the UART log. Throughput is `MiB/s` from `wolfcrypt/benchmark/benchmark.c`.

### Cross-board summary (auto-captured)

Captured by `bench_matrix.sh` driving `BOARD=<x> CONFIG=<y> TARGET=bench
flash` across all attached probes, then sliced from the matching
`/tmp/uart-monitor/latest/<NAME>_UART.log` post-flash window. Logs land in
`bench_logs/<board>_<config>.log` (or `bench_logs/<board>_cubemx_<config>.log`
when `-B cubemx` inserts the build-axis token). Run `python3 parse_bench.py` to regenerate
the tables below. `-` means: log absent, probe disconnected, run skipped
(M0/M0+ asm), board silicon wedged, or the bench timed out before the
algorithm's line printed.

#### Symmetric (MiB/s)

| Board | AES-128-CBC enc (bare) | AES-128-CBC enc (asm) | AES-128-CBC enc (c) | AES-128-CBC dec (bare) | AES-128-CBC dec (asm) | AES-128-CBC dec (c) | AES-128-GCM enc (bare) | AES-128-GCM enc (asm) | AES-128-GCM enc (c) | AES-128-GCM dec (bare) | AES-128-GCM dec (asm) | AES-128-GCM dec (c) | SHA-256 (bare) | SHA-256 (asm) | SHA-256 (c) | RNG (bare) | RNG (asm) | RNG (c) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `c5a3` | 15.20 | 1.030 | 0.762 | 14.99 | 1.036 | 0.760 | 1.045 | 0.667 | 0.492 | 1.027 | 0.674 | 0.481 | 1.994 | 2.266 | 1.982 | 0.732 | 0.783 | 0.741 |
| `f207` | 1.059 | 1.668 | 1.058 | 1.064 | 1.559 | 1.058 | 0.618 | 0.938 | 0.618 | 0.617 | 0.938 | 0.618 | 2.286 | 2.006 | 2.286 | 0.838 | 0.741 | 0.839 |
| `f437` | 11.45 | 1.775 | 1.088 | 11.40 | 1.623 | 1.132 | 11.02 | 1.037 | 0.699 | 1.455 | 1.036 | 0.699 | 25.73 | 2.388 | 2.631 | 1.603 | 0.859 | 0.961 |
| `f439` | 11.45 | 1.775 | 1.088 | 11.40 | 1.623 | 1.132 | 11.02 | 1.037 | 0.700 | 1.455 | 1.036 | 0.699 | 25.73 | 2.388 | 2.631 | 1.603 | 0.859 | 0.961 |
| `g071` | 0.098 | - | 0.098 | 0.098 | - | 0.098 | - | - | - | - | - | - | 0.136 | - | 0.136 | 0.055 | - | 0.055 |
| `g491` | 1.017 | 2.041 | 1.017 | 1.051 | 1.624 | 1.050 | 0.704 | 0.973 | 0.704 | 0.704 | 0.972 | 0.704 | 3.037 | 1.844 | 3.034 | 1.043 | 0.713 | 1.044 |
| `h7` | 16.07 | 1.998 | 1.620 | 15.92 | 1.982 | 1.657 | 15.26 | 1.310 | 0.970 | 1.839 | 1.312 | 0.969 | 30.54 | 3.128 | 4.138 | 1.787 | 1.064 | 1.376 |
| `h723` | 0.291 | 0.360 | 0.291 | 0.297 | 0.356 | 0.298 | 0.186 | 0.237 | 0.186 | 0.186 | 0.237 | 0.186 | 0.707 | 0.662 | 0.758 | 0.241 | 0.214 | 0.252 |
| `h7a3` | 1.164 | 1.446 | 1.164 | 1.216 | 1.439 | 1.215 | 0.746 | 0.956 | 0.746 | 0.746 | 0.956 | 0.744 | 2.694 | 2.829 | 2.805 | 0.947 | 0.875 | 0.983 |
| `h7s3` | 1.472 | 0.729 | 0.495 | 1.459 | 0.717 | 0.489 | 0.345 | 0.421 | 0.276 | 0.345 | 0.421 | 0.277 | 6.718 | 0.691 | 1.027 | 0.391 | 0.245 | 0.358 |
| `l4a6` | 0.917 | 0.362 | 0.229 | 0.910 | 0.342 | 0.237 | 0.165 | 0.166 | 0.119 | 0.165 | 0.166 | 0.119 | 3.561 | 0.303 | 0.321 | 0.214 | 0.113 | 0.122 |
| `u3` | 1.964 | 0.205 | 0.169 | 1.936 | 0.204 | 0.171 | 0.159 | 0.111 | 0.094 | 0.159 | 0.111 | 0.094 | 2.543 | 0.272 | 0.270 | 0.155 | 0.102 | 0.106 |
| `u585` | 11.05 | 0.818 | 0.655 | 10.87 | 0.819 | 0.655 | 0.741 | 0.482 | 0.385 | 0.741 | 0.482 | 0.383 | 12.83 | 1.299 | 1.247 | 0.943 | 0.485 | 0.492 |
| `wba52` | 8.439 | 0.430 | 0.336 | 8.284 | 0.424 | 0.344 | 0.422 | 0.259 | 0.210 | 0.422 | 0.259 | 0.211 | 7.097 | 0.769 | 0.743 | 0.532 | 0.284 | 0.285 |
| `wl55` | 2.176 | 0.365 | 0.230 | 2.144 | 0.345 | 0.235 | 0.185 | 0.167 | 0.119 | 0.184 | 0.167 | 0.119 | 0.327 | 0.303 | 0.327 | 0.125 | 0.113 | 0.125 |

#### Asymmetric (ops/sec)

ECDSA P-256 sign/verify only. Boards with HW V2 PKA (`u3` / `u585` /
`wba52` / `h7s3` and `c5a3` if RNG-recovered) show a clear step-up vs
`c` software. Boards without PKA give roughly identical numbers across
configs (SW math).

| Board | ECDSA P-256 sign (bare) | ECDSA P-256 sign (asm) | ECDSA P-256 sign (c) | ECDSA P-256 verify (bare) | ECDSA P-256 verify (asm) | ECDSA P-256 verify (c) |
|---|---|---|---|---|---|---|
| `c5a3` | 7.293 | - | 7.293 | 4.065 | - | 4.068 |
| `f207` | 6.168 | - | 6.178 | 3.390 | - | 3.393 |
| `f437` | 9.355 | - | 9.346 | 5.137 | - | 5.133 |
| `f439` | 9.355 | - | 9.346 | 5.137 | - | 5.133 |
| `g491` | 10.74 | - | 10.77 | 5.888 | - | 5.911 |
| `h7` | 12.09 | - | 10.79 | 6.678 | - | 5.900 |
| `h723` | 2.315 | - | 2.006 | 1.280 | - | 1.096 |
| `h7a3` | 9.099 | - | 8.170 | 5.042 | - | 4.481 |
| `h7s3` | 22.81 | - | 3.003 | 21.34 | - | 1.654 |
| `l4a6` | 1.160 | - | 1.159 | 0.638 | - | 0.638 |
| `u3` | 5.720 | - | 0.991 | 5.343 | - | 0.547 |
| `u585` | 34.19 | - | 4.415 | 32.17 | - | 2.438 |
| `wba52` | 34.88 | - | 2.725 | 33.04 | - | 1.512 |
| `wl55` | 3.026 | - | 1.161 | 1.529 | - | 0.639 |

The `asm` column is mostly `-` because the ASM build (Thumb2 SP-math
+ Dilithium) at 16-144 MHz on these parts does not complete its full
algorithm sweep within the 10-min per-run timeout used here, so the
ECC P-256 lines never print. Re-running with `bench_matrix.sh -t 1800`
would fill these in but at 5x the wall-clock cost.

#### PQC=1 (ML-KEM + ML-DSA, small-mem variants)

Captured with `make BOARD=<x> CONFIG=bare TARGET=bench PQC=1`. Small-mem variants trade speed for stack/heap footprint -- expect the `WOLFSSL_*_SMALL_MEM` numbers to be ~2-3x slower than the unconstrained variant but to fit on 256 KB / 320 KB SRAM parts. Listed in ops/sec.

| Operation           | H753 @ 480 MHz (M7F) | H7A3 @ 280 MHz (M7F) | G491 @ 170 MHz (M4F) | F207 @ 120 MHz (M3) | WBA52 @ 100 MHz (M33F) |
|---------------------|---------------------:|---------------------:|---------------------:|--------------------:|-----------------------:|
| ML-KEM 512  keygen  | 255.2                | 249.0                | 206.3                | 178.4               | 78.6                   |
| ML-KEM 512  encap   | 242.1                | 224.4                | 198.2                | 164.8               | 70.7                   |
| ML-KEM 512  decap   | 178.4                | 166.2                | 147.9                | 121.8               | 51.8                   |
| ML-KEM 768  keygen  | 154.2                | 150.3                | 125.6                | 107.8               | 47.2                   |
| ML-KEM 768  encap   | 141.2                | 132.7                | 115.8                |  96.7               | 41.7                   |
| ML-KEM 768  decap   | 108.6                | 102.6                |  89.9                |  74.7               | 32.0                   |
| ML-KEM 1024 keygen  |  92.0                |  90.4                |  75.1                |  64.8               | 28.5                   |
| ML-KEM 1024 encap   |  86.0                |  81.6                |  70.6                |  59.6               | 25.7                   |
| ML-KEM 1024 decap   |  69.4                |  66.2                |  57.5                |  48.1               | 20.7                   |
| ML-DSA 44   keygen  |  56.1                |  53.3                |  45.1                |  43.2               | 17.5                   |
| ML-DSA 44   sign    |  17.8                |  21.4                |  19.5                |  12.8               |  7.5                   |
| ML-DSA 44   verify  |  53.7                |  51.2                |  43.3                |  39.8               | 16.6                   |
| ML-DSA 65   keygen  |  31.5                |  30.2                |  25.2                |  24.5               | 10.0                   |
| ML-DSA 65   sign    |  11.1                |  11.8                | skipped              |   5.4               |  3.7                   |
| ML-DSA 65   verify  |  32.0                |  30.7                | skipped              |  24.0               | 10.0                   |
| ML-DSA 87   keygen  |  18.6                |  17.9                | skipped              |  14.5               | skipped                |
| ML-DSA 87   sign    |   9.5                |   9.6                | skipped              | skipped             | skipped                |
| ML-DSA 87   verify  |  18.5                |  17.9                | skipped              | skipped             | skipped                |

ML-DSA 87 + 65 partial skips: G491 96 KB / WBA52 128 KB / F207 128 KB RAM cannot hold the larger Dilithium contexts even with WOLFSSL_DILITHIUM_SMALL and the per-board stack carve-out (32 KB on G491 / F207, 48 KB on WBA52). The bench reports `Benchmark result: 0 (PASS)` after the levels that do fit.

Observation:

- The H7A3 @ 280 MHz numbers track H753 @ 480 MHz within 2-5% across almost every operation despite a 1.7x clock-rate gap. PQC throughput on Cortex-M7 saturates on the SHA-3 / SHAKE Keccak inner loop's memory-bandwidth-bound state-array shuffle, not on CPU clock -- so increasing M7 clock above ~280 MHz returns nearly nothing.
- The Cortex-M4F G491 @ 170 MHz beats the M33F WBA52 @ 100 MHz by ~2.6x while the clock ratio is only 1.7x. M4F outperforms M33F on PQC even per-clock; the M33 single-issue pipeline plus its TrustZone-required gating logic on every memory access costs a meaningful fraction of cycles on the Keccak inner loop.
- The plain Cortex-M3 F207 @ 120 MHz lands ~85% of the M4F G491 @ 170 MHz on a per-clock basis. FPU absence is irrelevant for PQC -- ML-KEM and ML-DSA are pure integer pipelines. M3 vs M4F is mostly the small ISA differences (saturating instructions, SDIV/UDIV in 2-4 cycles vs M3's slower divide) that the Keccak inner loop doesn't depend on heavily.
- The M4F line is ~70-80% of the M7F line on equivalent clocks-normalized basis. M7 dual-issue + cache wins on PQC but the gap is not the 4x-ish that integer benchmarks suggest -- Keccak's serial inner-loop dependencies prevent the M7 from utilizing both pipeline slots.

The full bench passes on all five boards (`Benchmark result: 0 (PASS)`) -- self-tests for ML-KEM and ML-DSA run before the bench timing loop and both verify clean.

#### Known caveats

- `c031` -- bench overflows 32 KB flash; the test-target SHA-256+AES smoke is the validation surface for this board.
- `f303`, `f767`, `h573`, `l552`, `l562`, `u083`, `u5`, `u545`, `wb55` -- probe not attached during sweep, or board wedged (u5 SWD examination failed -- needs power-cycle).
- `h5` -- known silicon damage on the lab board (ECC fault on instruction fetch -> NMI); a fresh H563 chip is needed.
- `n657` -- AP write error on flash (SRAM-load handshake state-wedge); BOOT0=VDD recovery procedure required.
- `<any>:asm` ECC entry -- wolfssl SP-math ASM (`sp_cortexm.c`) UNALIGNED-faults on first ECC op across every Cortex-M variant (garbage pointer at `sp_256_mont_sqr_8/mul_8` entry, upstream cause). Symmetric/HMAC/PBKDF2 in asm config complete and produce valid numbers; ECC sign/verify and later are blocked. `user_settings.h` default-undefs `WOLFSSL_SP_ARM_CORTEX_M_ASM` as a workaround so the asm config still completes via C SP-math.
- `<V2-PKA>:cubemx` ECDSA failure mode -- the wolfssl HW-PKA dispatch routes through ST's HAL `HAL_PKA_ECDSASign` (wolfssl's own static implementation in `stm32.c` is gated `#ifdef WOLFSSL_STM32_BARE` only). Two failure modes depending on silicon:
  - **Hang (wba52, h7s3)**: bench progresses cleanly through ECC keygen + ECDHE in cubemx, then hangs indefinitely at the first ECDSA sign call.
  - **Silent SW fallback (u3)**: bench completes but ECDSA verify stack rises to 4016 B (vs 1559 B in BARE), and perf falls to 0.66 ops/sec (vs 4.64 in BARE). The HAL_PKA HW path is never engaged; the wolfssl SP-math fastpath fires instead.
  BARE direct-register dispatch on the same chips works fine.

### Stack and heap measurements (STACK=1, CONFIG=bare)

Build with `STACK=1` to enable wolfssl's `HAVE_STACK_SIZE_VERBOSE` + `WOLFSSL_TRACK_MEMORY_VERBOSE`. The bench then emits a `[heap N bytes (M allocs), stack S bytes]` suffix on every algorithm line and a cumulative summary at the end. Tracker overhead: ~8.7 KB code, ~100 B BSS. The bare-metal adapter that paints the carved stack region lives in `boards/common/board_common.c` and is invoked from `main_bench.c` after `wolfCrypt_Init`. `bench_matrix.sh -S` drives this automatically: it re-runs each selected config with `STACK=1` into a companion `<stem>_stack.log`, whose per-algo stack/heap numbers `parse_bench.py` merges onto the same board/config (perf from the STACK=0 run, memory from the STACK=1 run).

Static (flash / RAM) footprint is separate from the per-iteration heap/stack above: `parse_size.py` runs `arm-none-eabi-size` over the `build/<board>-<build>-<target>-<config>/app.elf` outputs and emits a `.text` / `.data` / `.bss` table plus a callback-only flash-savings comparison (`TARGET=cbonly` vs the full-software `TARGET=dhuk` at the same board/config).

Currently four boards captured (h7 M7F, g491 M4F, wba52 M33F, f207 M3) in all three CONFIG paths (bare / asm / c). The headline tables below show the BARE config only (it has the most coverage and is the lead recommendation); asm and c numbers match BARE on every symmetric algorithm (same 368 B stack on AES/SHA across all three configs and all four boards). Sweep widens to more boards and the cubemx axis as silicon time permits.

**Headline finding (HW PKA -- V1 AND V2):** ECDSA verify on any STM32 HW-PKA chip uses **~1500 B stack vs 3872-4016 B** on every SW-only board. The HW PKA path takes the SP-math precomp table off the stack entirely. Reproduces across:

| Chip            | CPU    | PKA gen | Verify stack |
|-----------------|--------|---------|--------------|
| STM32WL55JC     | M4F    | **V1**  | 1560 B       |
| STM32WBA52      | M33F   | **V2**  | 1560 B       |
| STM32U385       | M33F   | **V2**  | 1559 B       |
| STM32H7S3       | M7F    | **V2**  | 1432 B       |

Four different chips, four different CPU families, two PKA generations -- the stack drop is consistent across the entire HW PKA design space. Definitively a HW-PKA effect, not a board / silicon / PKA-generation quirk. SW-only boards (h7, g491, f207) stay at 3872-4016 B because they execute the full SP-math precomp on stack.

**ASM config regression:** the wolfssl SP-math ASM (`sp_cortexm.c`) hardfaulted on the first ECC operation across every Cortex-M variant tested (M3/M4F/M33F/M7F -- CFSR=UNFAULTED for unaligned access on the leaf SP-math, root cause upstream). `user_settings.h` default-undefs `WOLFSSL_SP_ARM_CORTEX_M_ASM` under `CONFIG=asm` so SP-math falls back to `sp_c32.c` and the bench completes; thumb2 symmetric ASM is unaffected. Re-enable with `-DWOLFSSL_SP_ARM_CORTEX_M_ASM_DANGEROUS` for upstream debugging.

**BARE vs CubeMX/HAL dispatch:** BARE direct-register dispatch beats CubeMX/HAL by ~2x on the same HW block, because every HAL call adds function-call + locking + state-machine overhead per crypto operation. Headline numbers from the same silicon, same crypto block, only the dispatch path changing:

| Algorithm    | Board (HW)      | BARE (MiB/s) | CubeMX/HAL (MiB/s) | Speedup |
|--------------|-----------------|--------------|--------------------|---------|
| AES-128-CBC  | h7 CRYP         | 19.90        | 9.35               | **2.1x**  |
| AES-128-CBC  | wba52 SAES      | 7.97         | 4.35               | **1.8x**  |
| AES-128-GCM  | h7 CRYP         | 18.24        | 8.63               | 2.1x    |
| SHA-256      | h7 HASH         | 25.44        | 31.47              | 0.8x (HAL slightly better -- whole-block streaming) |

ECDSA P-256 sign/verify see no BARE-vs-CubeMX delta (~10.7 ops/sec on h7) because the SP-math fastpath bypasses the HAL on both: those boards have no V1/V2 PKA arming. wba52, where HW V2 PKA is active, lifts ECDSA sign from 2.5 ops/sec (SW C) to 33 ops/sec (BARE direct PKA, **13x**).

On boards without dedicated crypto HW (`g491`, `f207`), all four configs are equivalent: ~1.0 MiB/s AES-128-CBC, ~6-11 ops/sec ECDSA sign. The CubeMX column matches BARE byte-for-byte because there's no HW dispatch difference.

#### Stack per algorithm (bytes per bench iteration)

| Board | Arch  | HW PKA | CBC enc | GCM enc | SHA-256 | ECDSA sign | ECDSA verify |
|-------|-------|--------|---------|---------|---------|------------|--------------|
| `h7`    | M7F   | none   | 368     | 368     | 368     | 2408       | 3872         |
| `h7s3`  | M7F   | **V2** | 368     | 368     | 368     | 3088       | **1432 (HW)** |
| `h7a3`  | M7F   | none   | 368     | 368     | 368     | 2408       | 3872         |
| `h723`  | M7F   | none   | 368     | 368     | 368     | 2408       | 3872         |
| `u3`    | M33F  | **V2** | 368     | 368     | 368     | 3280       | **1559 (HW)** |
| `g491`  | M4F   | none   | 368     | 368     | 368     | 2552       | 4016         |
| `wba52` | M33F  | **V2** | 368     | 368     | 368     | 2760       | **1560 (HW)** |
| `c5a3`  | M33F  | none*  | 368     | 368     | 368     | 2552       | 4016         |
| `f439`  | M4F   | none   | 368     | 368     | 368     | 2552       | 4016         |
| `wl55`  | M4F   | **V1** | 368     | 368     | 368     | 2552       | **1560 (HW)** |
| `l4a6`  | M4F   | none   | 368     | 368     | 368     | 2552       | 4016         |
| `f207`  | M3    | none   | 368     | 368     | 368     | 2552       | 4016         |
| `g071`  | M0+   | none   | **425** | -       | **425** | -          | -            |

Symmetric stack is byte-constant on M3 / M4F / M7F / M33F (368 B), but rises to **425 B on M0+** (g071) due to fewer GP registers forcing more spills. AES-128-GCM column for g071 shows `-` because the bench was not built (GCM disabled on g071 to fit 128 KB flash); ECDSA columns show `-` because g071 does not build RSA/ECC at all. Asymmetric stack varies with SP-math intermediate count; HW-PKA verify rows (`u3`, `wba52`, `wl55`, `h7s3`) drop 60-65% because the HW PKA path bypasses SP-math entirely on verify. The pattern reproduces across four chips spanning three CPU families (M7F + M4F + M33F) and both PKA generations (V1 + V2), so it's clearly a HW-PKA dispatch effect, not a board or family quirk.

*c5a3 has V2 PKA silicon but the PKA IP is not yet engaged in the BARE port (two open IP issues in the C5 family -- see `reference_stm32c5a3_bare` memory note). Its ECDSA verify stack 4016 B reflects the SW SP-math path; once PKA HW is wired up, expect 1560 B like other V2 PKA chips.

#### Heap per algorithm (bytes per bench iteration)

| Board | Arch  | CBC enc | GCM enc | SHA-256 | ECDSA sign | ECDSA verify |
|-------|-------|---------|---------|---------|------------|--------------|
| `h7`    | M7F   | 0       | 0       | 0       | 0          | 0            |
| `h7s3`  | M7F   | 0       | 0       | 0       | 0          | 0            |
| `h7a3`  | M7F   | 0       | 0       | 0       | 0          | 0            |
| `h723`  | M7F   | 0       | 0       | 0       | 0          | 0            |
| `u3`    | M33F  | 0       | 0       | 0       | 0          | 0            |
| `g491`  | M4F   | 0       | 0       | 0       | 0          | 0            |
| `wba52` | M33F  | 0       | 0       | 0       | 0          | 0            |
| `c5a3`  | M33F  | 0       | 0       | 0       | 0          | 0            |
| `f439`  | M4F   | 0       | 0       | 0       | 0          | 0            |
| `wl55`  | M4F   | 0       | 0       | 0       | 0          | 0            |
| `l4a6`  | M4F   | 0       | 0       | 0       | 0          | 0            |
| `f207`  | M3    | 0       | 0       | 0       | 0          | 0            |
| `g071`  | M0+   | 0       | -       | 0       | -          | -            |

Per-iteration heap is 0 on the symmetric and ECC paths because SP-math + `NO_WOLFSSL_SMALL_STACK` keep all working buffers on the stack. RNG init does a one-shot 128 B alloc on each board; not shown.

#### Cumulative peak concurrent heap (bytes, whole bench)

| Board | Arch  | Peak | Allocs balanced |
|-------|-------|------|-----------------|
| `h7`    | M7F   | 2368 | 1339 / 1339 (no leak) |
| `h7s3`  | M7F   | 2368 | 303 / 303   |
| `h7a3`  | M7F   | 2368 | 681 / 681   |
| `h723`  | M7F   | 2368 | 177 / 177   |
| `u3`    | M33F  | 2368 | 104 / 104   |
| `g491`  | M4F   | 2368 | 775 / 775   |
| `wba52` | M33F  | 2368 | 348 / 348   |
| `c5a3`  | M33F  | 2368 | 522 / 522   |
| `f439`  | M4F   | 2368 | 1138 / 1138 |
| `wl55`  | M4F   | 2368 | 88 / 88     |
| `l4a6`  | M4F   | 2368 | 140 / 140   |
| `f207`  | M3    | 2368 | 605 / 605   |
| `g071`  | M0+   | **2208** | 46 / 46    |

Peak is **2368 B across all eleven RSA/ECC-enabled boards** -- platform-independent for any board that builds the full crypto suite. The g071 outlier at 2208 B reflects its trimmed configuration: g071 has only 128 KB flash and drops RSA/ECC/DH (per memory note `reference_stm32g071_bare`), so its largest SP-math intermediate is smaller. All allocs are matched by deallocs at `wolfCrypt_Cleanup`. Per-board total alloc count varies (46-1339) because boards with fewer enabled algorithms make fewer total allocations, but every board with the full RSA/ECC suite converges on the same concurrent peak.

### BARE vs CubeMX HAL comparison (MiB/s and ops/sec)

`BUILD=cubemx` swaps boards/<x>/hw_init.c for hw_init_cubemx.c, undefs
`WOLFSSL_STM32_BARE`, defines `WOLFSSL_STM32_CUBEMX`, and links ST's
HAL drivers. wolfcrypt's port/st/stm32.c then dispatches to
`HAL_CRYP_Encrypt` / `HAL_HASH_*` / `HAL_RNG_*` / `HAL_PKA_*` instead
of touching CRYP / HASH / RNG / PKA registers directly. All else --
clock setup, UART, bench inputs -- is byte-identical between BUILD
flavors so the delta isolates the HAL overhead.

Captured with `bench_matrix.sh -B cubemx -c bare`. Logs at
`bench_logs/<board>_cubemx_bare.log`. The DHUK boards (`u3`, `u545`,
`u585`, `c5a3`) were refreshed at 96/144 MHz on this branch.

`c5a3` is a special case: C5 ships ST's new-generation HAL, which has no
classic CRYP/HASH/PKA/SAES driver, so the cubemx build uses the new-gen
HAL for board bring-up only and wolfcrypt stays on the direct-register
crypto path. Its bare and cubemx columns are therefore identical (same
register crypto), which the numbers confirm. The transparent DHUK and
CCB paths likewise share one SAES register sequence across BUILD
flavors, so DHUK/CCB throughput equals the bare column on every DHUK
board (the bench schedule above does not separately time the DHUK
crypto-callback; correctness is covered by `TARGET=dhuk`/`ccb`).

| Board | SYSCLK | AES-128-CBC enc bare | AES-128-CBC enc cubemx | AES-128-GCM enc bare | AES-128-GCM enc cubemx | SHA-256 bare | SHA-256 cubemx | ECDSA P-256 sign bare | ECDSA P-256 sign cubemx |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `h7`   | 480 MHz | 16.07 | 10.12   | 15.26 | 8.78   | 30.54 | 33.15 | 12.09 | 10.88 |
| `f439` | 144 MHz | 11.45 |  7.29   | 11.02 | 6.65   | 25.73 | 25.68 |  9.36 |  9.36 |
| `u585` |  96 MHz | 11.55 |  5.61   |  0.73 | 5.22   | 12.76 | 21.97 | 34.16 | 34.45 |
| `u545` |  96 MHz | 11.18 |  5.40   |  0.73 | 5.02   | 12.52 | 20.21 | 34.16 | 34.45 |
| `u3`   |  96 MHz |  9.46 |  4.91   |  0.50 | 4.41   |  7.04 | 11.66 | 33.83 |  5.48 |
| `c5a3` | 144 MHz | 16.56 | 16.58   |  1.05 | 1.05   |  2.00 |  1.99 | 51.03 | 51.03 |
| `g491` | 170 MHz |  1.02 |  1.02   |  0.70 | 0.70   |  3.04 |  3.03 | 10.74 | 10.78 |
| `h723` |  64 MHz |  0.29 |  0.47   |  0.19 | 0.26   |  0.71 |  1.02 |  2.32 |  2.99 |
| `h7a3` | 280 MHz |  1.16 |  0.64   |  0.75 | 0.40   |  2.69 |  1.52 |  9.10 |  4.26 |
| `wl55` |  16 MHz |  2.18 |  0.87   |  0.18 | 0.14   |  0.33 |  0.33 |  3.03 |  1.16 |
| `wba52`| 100 MHz |  8.44 |  4.40   |  0.42 | 0.42   |  7.10 | 15.63 | 34.88 |  HW-OK\* |
| `h7s3` |  64 MHz |  1.47 |  1.92   |  0.35 | 0.42   |  6.72 |  6.52 | 22.81 |  HW-OK\* |

CubeMX HAL overhead per algorithm class (h7/f439 averaged):
- AES-128-CBC: 36-37% slower under HAL (extra struct setup + memcpy per call)
- AES-128-GCM: 41% slower under HAL on h7/f439; **u585 is 7x faster** under HAL because BARE U5 GCM does SW GHASH + HW ECB while HAL drives full HW GCM
- SHA-256: roughly identical (HAL_HASH is a thin shim)
- ECDSA P-256 sign: identical on h7/f439 (both go through SW SP-math; H753 and F439 have no PKA)

`u545` / `u585` cubemx asymmetric cells need a long sweep (`-t 1800`): the
cubemx RNG-conditioning stall (see "Cubemx silicon caveats" below) inflates
the per-test reseed, so the default-timeout sweep expires in the symmetric
section. With the longer sweep both complete -- ECDSA P-256 sign is 34.45
ops/sec on both, equal to their bare HW-PKA columns (34.16), confirming the
CubeMX HAL-PKA path reaches HW parity on the U5 family. The bare path is
unaffected by the stall.

Some interesting deltas in the newly-validated rows:
- `u3` cubemx ECDSA P-256 sign is **6x slower** than BARE (5.48 vs
  33.83 ops/sec). BARE u3 uses HW V2 PKA; cubemx u3 has PKA disabled
  (wolfssl's stm32.c PKA hal_pka.h include-chain is missing U3 -- see
  user_settings.h note) so it falls back to SW SP-math. Once that
  upstream chain adds STM32U3, cubemx should match BARE.
- `c5a3` bare and cubemx columns match to within noise (16.56 vs 16.58
  CBC, 51.03 vs 51.03 ECDSA sign): the C5 cubemx build runs the same
  direct-register crypto as bare, so there is no HAL overhead to pay.
- `u3` / `u545` / `u585` AES-128-GCM is **7-9x faster** under HAL (e.g.
  u3 0.50 -> 4.41) because BARE U5 GCM does SW GHASH + HW ECB while the
  HAL drives the full HW GCM block; SHA-256 is likewise faster under HAL
  (HW HASH vs BARE SW SHA).
- `g491` and `h723` show **cubemx slightly faster than BARE** on
  ECDSA sign + SHA-256. Both chips have no AES/HASH/PKA HW so both
  paths run SW math; the cubemx binary's HAL_Init enables ICACHE /
  DCACHE / PREFETCH which BARE leaves alone, giving a small uplift.
- `h723` AES-128-CBC enc cubemx **1.6x faster than BARE** (0.47 vs
  0.29 MiB/s) -- pure-SW AES on M7 with ICACHE on (cubemx) beats
  ICACHE-off (BARE).

### Cubemx silicon caveats

- `wba52` / `h7s3` / `c5a3` cubemx: HAL_RNG_GenerateRandomNumber
  times out from wolfssl's CUBEMX wc_GenerateSeed path -- the
  newer RNG cells on these chips need a CONDRST / NIST-conditioning
  init sequence that HAL_RNG_Init does not perform in single-shot
  mode (CONDRST gets stuck high). BARE wc_GenerateSeed has the
  right sequence (random.c C5 NIST init block) but it is only on
  the BARE path. Workaround applied in user_settings.h:
  `#undef STM32_RNG` for these boards under STM32_BUILD_CUBEMX so
  wolfssl falls back to its software DRBG. Cubemx bench now
  reaches the full algo sweep, just without HW RNG.
- `wba52` / `h7s3` cubemx ECC: was reporting "Benchmark ECDSA
  [SECP256R1] sign 256 failed: -248" -- root-caused as missing
  HAL_PKA_Init() in hw_init_cubemx.c (Track D1 RESOLVED). Once
  application calls HAL_PKA_Init(&hpka) after clock/uart init,
  ECDSA HW PKA works on every V2 board. wba52 / u585 / h7s3
  silicon-validated post-fix; e.g. u585 cubemx ECDSA P-256 sign
  now does 34.45 ops/sec vs BARE u585 34.19 ops/sec (effectively
  identical). The -248 was NOT a multi-curve quirk in wolfssl --
  it was just an uninitialized PKA handle.
- `l4a6` cubemx: LPUART1 output is garbled regardless of source
  (HSI vs PCLK1 tried) -- baud-rate / fck-lookup mismatch. l4a6
  BARE works fine. Likely L4 HAL LPUART BRR calculation issue.
- `u545` / `wb55` cubemx: build clean but probes not currently
  attached on the bench; silicon validation pending.
- All boards listed here have hw_init_cubemx.c in-tree and build
  clean -- the gaps are runtime-only.

Wiring notes per board are in the section header comment of each
`boards/<x>/hw_init_cubemx.c`. The harness-wide HAL pieces:
- `boards/common/board_common.c` `SysTick_Handler` now forwards to
  `HAL_IncTick()` under CUBEMX so HAL's internal ms tick advances
  (without this, every HAL_* call with HAL_MAX_DELAY timeout hangs
  forever because `uwTick` never increments).
- `user_settings.h` defines `STM32_HW_CLOCK_AUTO` under CUBEMX so
  wolfcrypt's `wc_Stm32_*_Init()` helpers auto-enable CRYP / HASH /
  RNG / PKA peripheral clocks via `__HAL_RCC_*_CLK_ENABLE()`.
- On TinyAES families (U5/U3/L4/L5/G0/G4) `user_settings.h` aliases
  the missing `__HAL_RCC_CRYP_CLK_ENABLE` to `__HAL_RCC_AES_CLK_ENABLE`
  (wolfssl only does this auto-alias for H5/C5 today).

### NUCLEO-F439ZI -- STM32F439ZI (Cortex-M4F, 144 MHz)

| Algorithm           | C (sw)         | ASM (Thumb2)   | BARE (CRYP HW)  |
|---------------------|----------------|----------------|-----------------|
| AES-128-CBC enc     |  1.113 MiB/s   |  1.775 MiB/s   | **11.401 MiB/s**|
| AES-128-CBC dec     |  1.144         |  1.632         | **11.353**      |
| AES-128-GCM enc (whole) |  723 KiB/s |  1.031 MiB/s   | **10.840 MiB/s**|
| AES-128-GCM dec (sw GHASH + HW ECB) | 722 KiB/s | 1.030 | 1.439         |
| AES-256-CBC enc     |  841 KiB/s     |  1.282 MiB/s   | **10.929 MiB/s**|
| AES-256-GCM enc (whole) |  588 KiB/s |  861 KiB/s     | **10.404 MiB/s**|
| MD5                 |  9.780 MiB/s   |  9.780 MiB/s   | **25.854 MiB/s**|
| SHA-1               |  5.097         |  5.087         | **25.830**      |
| SHA-256             |  2.631         |  2.395         | **25.757**      |
| SHA-384             |  1.157         |  1.113         |  1.157 (sw \*) |
| SHA-512             |  1.157         |  1.113         |  1.158 (sw \*) |
| HMAC-SHA256         |  2.607         |  2.376         | **25.439**      |

\* F4 HASH IP supports only MD5/SHA1/SHA224/SHA256 in hardware. SHA-384/512
fall through to software (no acceleration possible on this chip).

Notes:
- AES-GCM **decrypt** in BARE uses the SW path (HW ECB blocks underneath)
  for tag-verification semantics; CRYP HW GCM phase machine is encrypt-only
  in this v1 driver.
- AES-GCM **encrypt** at 10.8 MiB/s exercises the HW GCM phase machine
  (init/header/payload/final). For partial-block PT or non-12B IV the
  driver returns `CRYPTOCB_UNAVAILABLE` and aes.c falls back to SW GHASH +
  HW ECB.

### NUCLEO-H753ZI -- STM32H753ZI (Cortex-M7F, 480 MHz via PLL)

HSE 8 MHz BYPASS (ST-LINK MCO) -> PLL1 (M=1, N=120, P=2) -> SYSCLK 480 MHz,
HCLK 240 MHz, PCLK 120 MHz. VOS Scale 1 + ODEN (overdrive). I/D-cache OFF.

| Algorithm           | C (sw)         | ASM (Thumb2)   | BARE (CRYP HW)  |
|---------------------|----------------|----------------|-----------------|
| AES-128-CBC enc     |  1.626 MiB/s   |  1.986 MiB/s   | **19.165 MiB/s**|
| AES-128-CBC dec     |  1.637         |  1.988         | **18.970**      |
| AES-256-CBC enc     |  1.203         |  1.449         | **18.359**      |
| AES-128-GCM enc (whole) |  1000 KiB/s |  1.305 MiB/s  | **18.457 MiB/s**|
| AES-128-GCM dec (sw GHASH + HW ECB) | 1.001 MiB/s | 1.305 | 1.945     |
| AES-256-GCM enc (whole) |  824 KiB/s  |  1.037 MiB/s  | **18.408 MiB/s**|
| MD5                 | 13.843 MiB/s   | 13.122 MiB/s   | **26.025 MiB/s**|
| SHA-1               |  7.756         |  7.788         | **25.977**      |
| SHA-256             |  4.065         |  3.192         | **25.928**      |
| SHA-384             |  1.628         |  2.277         |  1.637 (sw \*) |
| SHA-512             |  1.645         |  2.279         |  1.649 (sw \*) |
| HMAC-SHA256         |  4.004 MiB/s   |  3.213 MiB/s   | **25.464 MiB/s**|
| ChaCha20            |  6.396         |  9.253         |  7.007          |
| Poly1305            | 20.634         | 33.081         | 20.190          |

\* H7 HASH IP supports MD5/SHA1/SHA224/SHA256 only. SHA-384/512 fall
through to software (no acceleration on H743/H753; H7B3 has SHA-512 HW).

Notes:
- AES via CRYP HW caps at ~19 MiB/s -- bottleneck is the AHB2 bus to the
  CRYP peripheral (240 MHz) plus FIFO turnover, not CPU clock. SHA-256
  via HASH HW similarly caps at ~26 MiB/s.
- M7F dual-issue: ASM Thumb2 wins ChaCha/Poly1305 because the M7's
  superscalar pipeline gets more out of hand-scheduled assembly than
  the compiler does. AES/SHA still go straight through HW.
- Enabling I/D-cache and ART accelerator should add another ~30-50% on
  the SW paths (and zero on HW paths since those are bus-bound). Left
  off here so numbers reflect the worst-case "HAL-free, no caches" line.

### NUCLEO-WB55RG -- STM32WB55RG (Cortex-M4F, 64 MHz via PLL)

HSI16 -> PLL (M=1, N=8, R=2) -> SYSCLK 64 MHz, HCLK = PCLK = 64 MHz.
HSI48 enabled as RNG kernel clock. WB55 has TinyAES on AHB2 (AES1, the
M4 application instance) and **no HASH peripheral** -- SHA falls back
to software.

| Algorithm           | C (sw)         | ASM (Thumb2)   | BARE (TinyAES HW) |
|---------------------|----------------|----------------|-------------------|
| AES-128-CBC enc     |  428 KiB/s     |  920 KiB/s     | **7.237 MiB/s**   |
| AES-128-CBC dec     |  440 KiB/s     |  700 KiB/s     | **7.178 MiB/s**   |
| AES-256-CBC enc     |  313 KiB/s     |  666 KiB/s     | **5.994 MiB/s**   |
| AES-128-GCM enc     |  298 KiB/s     |  392 KiB/s     | **740 KiB/s**     |
| AES-128-GCM dec     |  298 KiB/s     |  392 KiB/s     |   740 KiB/s       |
| AES-256-GCM enc     |  237 KiB/s     |  336 KiB/s     | **716 KiB/s**     |
| MD5                 |  3.495 MiB/s   |  3.509 MiB/s   |  3.509 MiB/s (sw) |
| SHA-1               |  1.567         |  1.561         |  1.565 (sw)       |
| SHA-256             |  1.247         |   732 KiB/s    |  1.243 (sw)       |
| SHA-384             |   377 KiB/s    |  358 KiB/s     |   378 KiB/s (sw)  |
| SHA-512             |   378 KiB/s    |  358 KiB/s     |   378 KiB/s (sw)  |
| HMAC-SHA256         |  1.234 MiB/s   |   726 KiB/s    |  1.229 MiB/s (sw) |
| ChaCha20            |  2.107         |  2.891         |  2.111            |
| Poly1305            |  8.049         | 11.133         |  8.049            |

Notes:
- WB55 has no HASH peripheral, so SHA-* numbers are software in all
  three columns. The slight ASM regression on SHA-256/HMAC-SHA256 is
  the same superscalar mismatch story as on M4 generally -- the
  hand-scheduled ASM is slightly slower than what gcc emits for these
  block primitives on this core.
- AES-GCM via TinyAES doesn't get the dramatic speedup that AES-CBC
  does because the bench mixes whole-block and partial-block GCM
  cases. Whole-block 12-byte-IV GCM hits the HW phase machine
  (init/header/payload/final), but partial blocks fall back to SW
  GHASH plus HW ECB and that fallback dominates the bench mean.
- AES1 lives on AHB2 alongside the M4. AES2 (on AHB3) is reserved
  for the M0+ radio core / shared use; touching it from M4 in normal
  operation is undefined.

### NUCLEO-G491RE -- STM32G491RE (Cortex-M4F, 170 MHz via PLL)

HSI16 -> PLL (M=4, N=85, R=2) -> SYSCLK 170 MHz. VOS Range 1 boost mode
(PWR_CR5.R1MODE = 0) is required for HCLK > 150 MHz on G4. FLASH 4 WS
+ prefetch + I/D-cache. **G491RE has no AES peripheral and no HASH;
the AES block is only on the G4A1xx variant in this G4 sub-family.**
RNG (HSI48 kernel clock) and PKA hardware are present.

| Algorithm           | C (sw)         | ASM (Thumb2)   | BARE (no HW here) |
|---------------------|----------------|----------------|-------------------|
| AES-128-CBC enc     |  1.017 MiB/s   |  2.039 MiB/s   |  1.017 MiB/s      |
| AES-128-CBC dec     |  1.050         |  1.624         |  1.052            |
| AES-256-CBC enc     |   770 KiB/s    |  1.475 MiB/s   |   770 KiB/s       |
| AES-128-GCM enc     |   718 KiB/s    |   987 KiB/s    |   718 KiB/s       |
| AES-256-GCM enc     |   576 KiB/s    |   835 KiB/s    |   575 KiB/s       |
| MD5                 |  8.455 MiB/s   |  8.455 MiB/s   |  8.455 MiB/s      |
| SHA-1               |  4.118         |  4.114         |  4.114            |
| SHA-256             |  3.031         |  1.844         |  3.037            |
| SHA-384             |   996 KiB/s    |   957 KiB/s    |   996 KiB/s       |
| SHA-512             |   996         |   957          |   996             |
| HMAC-SHA256         |  3.003 MiB/s   |  1.829 MiB/s   |  3.009 MiB/s      |
| ChaCha20            |  4.956         |  7.275         |  4.951            |
| Poly1305            | 18.457         | 26.538         | 18.481            |
| ECDHE secp256r1     | 11.8 ops/s     | 11.9 ops/s     | 11.8 ops/s        |

Notes:
- BARE column is identical to C because G491RE has no AES/HASH HW.
  BARE on this board only accelerates RNG. PKA hardware is not present
  on G491RE either (only G4A1xx in the same line has it), so the
  bare-metal PKA driver can't be validated here.
- ASM Thumb2 wins AES-CBC/GCM and ChaCha/Poly1305. SHA-256 is slower
  in ASM than C on M4 -- the hand-scheduled Thumb2 isn't as good as
  what gcc emits for SHA-256 specifically.

### Per-board template (fill in as boards come online)

Format the per-board section as:

```
### <BOARD-NAME> -- <CHIP> (<CPU>, <SYSCLK> MHz)

| Algorithm | C | ASM | BARE |
| ... |
```



## Targets

| TARGET  | Source                                |
|---------|---------------------------------------|
| `test`  | `wolfcrypt/test/test.c` - KAT vectors |
| `bench` | `wolfcrypt/benchmark/benchmark.c`     |
| `dhuk`  | `src/main_dhuk.c` - DHUK ECC sign + GMAC (u3/u585/u545) |
| `ccb`   | `src/main_ccb.c` - CCB-protected ECDSA via wc_ecc_sign_hash (u3, bare + cubemx) |
| `ccbhal`| `src/main_ccbhal.c` - ST HAL_CCB reference flow (u3, cubemx) |
| `cbonly`| `src/main_cbonly.c` - callback-only ECDSA + AES-GCM + HMAC-SHA256 + TRNG on HW, `STM32_BARE_CB_ONLY` flash-strip (u3/u585/u545/c5a3/c562) |
| `aesplain`| `src/main_aesplain.c` - plaintext-key AES vs DHUK-seed AES selected per `Aes` by devId (u3/u585/u545/c5a3/c562) |
| `plaingcm`| `src/main_plaingcm.c` - direct `wc_Stm32_Aes_Gcm()` HW AES-GCM KAT, no SW fallback (f437/f439/h7/h7s3/u3/u585/u545/l4a6/l562/wba52/n657) |
| `cubeaes`| `src/main_cubeaes.c` - CubeMX AES crypto-callback AES-GCM KATs, `WOLF_CRYPTO_CB_ONLY_AES` (u3, cubemx) |
| `cubecrypto`| `src/main_cubecrypto.c` - CubeMX HW ECDSA (PKA) + CCB ECDSA + AES-GCM all via the callback (u3, cubemx) |

## Prerequisites

- `arm-none-eabi-gcc`
- OpenOCD with the appropriate target config (`stm32h5x.cfg` for H5,
  `stm32f4x.cfg` for F437)
- ST CMSIS device headers (downloaded via STM32CubeIDE / STM32CubeMX):
  - `~/STM32Cube/Repository/STM32Cube_FW_H5_V1.5.1` for H5
  - `~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3` for F437

## Build

```sh
# H563 + bare-metal HW (HASH/RNG)
make BOARD=h5 CONFIG=bare TARGET=test

# H563 + pure C baseline
make BOARD=h5 CONFIG=c    TARGET=test

# Benchmark
make BOARD=h5 CONFIG=bare TARGET=bench
```

## Flash and watch UART

```sh
make BOARD=h5 CONFIG=bare TARGET=test flash
# UART output appears on the ST-LINK virtual COM port (USART3 PD8/PD9 @ 115200)
```

## Layout

```
STM32_Bare_Test/
  Makefile
  user_settings.h               # multi-board wolfSSL config
  include/board.h               # portable board API (board_init, board_uptime_ms, ...)
  src/main_test.c               # KAT entry, uses board.h only
  src/main_bench.c              # benchmark entry, uses board.h only
  src/stubs.c                   # _sbrk, time() stubs
  boards/h5/                    # NUCLEO-H563ZI
    startup_stm32h563xx.s       # CMSIS device template
    stm32h563_flat.ld           # CMSIS linker script (FLASH variant)
    system_stm32h5xx.c          # CMSIS SystemInit
    hw_init.c                   # bare-metal RCC + USART3 + SysTick
  boards/f437/                  # STM32439I-EVAL (TODO)
```

## Adding a new board

1. Pick a short BOARD name (e.g., `u5`).
2. Add a `boards/<name>/` dir with: `startup_*.s`, `*_flat.ld`,
   `system_*.c`, `hw_init.c` (implements `board_init`,
   `board_sysclk_hz`, `board_uptime_ms`, `board_name`).
3. Add a `STM32_BOARD_<name>` arm to `user_settings.h`.
4. Add a `BOARD=<name>` block to the Makefile (CMSIS paths, MCU flags,
   linker script, OpenOCD target).
