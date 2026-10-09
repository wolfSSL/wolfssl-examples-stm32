# wolfDemo wolfCrypt Benchmark & Test

Bare-metal wolfCrypt benchmark and self-test for the **wolfDemo** board
(STM32U585CIT6). Demonstrates wolfCrypt with the STM32U5 hardware crypto
engines (AES, HASH, PKA and TRNG).

The benchmark runs in a loop with a 5 second pause between runs. Each line of
output steps the LED "bark bars" along, and the output can be shown as a live
table with [bench_tui.py](../bench_tui.py).

## Board

| Item | Value |
|------|-------|
| MCU | STM32U585CIT6 (Cortex-M33, 160 MHz) |
| Flash | 2 MB |
| SRAM | 768 KB (+ 16 KB SRAM4, unused) |
| UART | USART1 - PA9 (TX), PA10 (RX) via the CH340G on the USB-C port, 115200 8N1 |
| Clock | HSE 8 MHz -> PLL1 -> 160 MHz, HSI48 for the TRNG |
| Crypto HW | AES (TinyAES), HASH, PKA, TRNG |
| LEDs | PB12, PB13, PB14, PB15 (right to left) |
| Buttons | BT1 - PB4, BT2 - PB5 |

## Requirements

* [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
  (`arm-none-eabi-gcc`) 11.3 or newer. The linker script uses the `READONLY`
  keyword, which needs binutils 2.38 or newer.
* A wolfSSL source tree. The latest release
  ([v5.9.4-stable](https://github.com/wolfSSL/wolfssl/releases)) or `master`.
  By default the Makefile expects it to be checked out next to this
  repository.
* The STM32CubeU5 firmware package. STM32CubeMX / STM32CubeIDE download this
  to `~/STM32Cube/Repository/`, or it can be cloned from
  [GitHub](https://github.com/STMicroelectronics/STM32CubeU5) with its
  submodules. The Makefile defaults to V1.8.0, set `STM32CUBE_FW_U5` for any
  other version (this example was tested with V1.7.0).
* For flashing: [OpenOCD](https://openocd.org/) with an ST-Link or J-Link on
  the JTAG header, or the UART bootloader (see below).

## Quick Start

```bash
# Build the wolfCrypt benchmark (STM32 HW crypto)
make

# Flash via OpenOCD with an ST-Link
make flash

# Flash via OpenOCD with a J-Link
make flash OPENOCD_INTERFACE=jlink

# Build and flash the wolfCrypt self-test
make test
make flash TARGET=test

# Pure software benchmark, for comparison
make CONFIG=c bench
make flash CONFIG=c
```

If wolfSSL or STM32CubeU5 are somewhere else:

```bash
make WOLFSSL_ROOT=$HOME/src/wolfssl \
     STM32CUBE_FW_U5=$HOME/STM32Cube/Repository/STM32Cube_FW_U5_V1.7.0
```

`TARGET` and `CONFIG` can also be given as variables, for example
`make TARGET=test CONFIG=c`.

## Build Variants

| CONFIG | AES | HASH | ECDSA | RSA / DH / ECC math | Description |
|--------|-----|------|-------|---------------------|-------------|
| `hw` (default) | STM32 AES | STM32 HASH | STM32 PKA | SP Cortex-M assembly | Hardware crypto |
| `c` | Software | Software | Software | SP C (sp_c32.c) | Pure C, no acceleration |

Both variants seed the DRBG from the STM32 TRNG.

| TARGET | Description |
|--------|-------------|
| `bench` (default) | wolfCrypt benchmark, repeated forever. Each line includes the heap and stack used |
| `test` | wolfCrypt self-test, run once |

## Algorithms Enabled

- **Symmetric**: AES-128/256 (CBC, GCM, CCM, CTR, ECB, CMAC, GMAC),
  ChaCha20-Poly1305. AES-192 is only available with `CONFIG=c` because the
  TinyAES engine does not support 192-bit keys.
- **Hash**: SHA-256, SHA-384, SHA-512, SHA-3, SHAKE128/256, HMAC, HKDF
- **RSA**: 2048-bit (SP math)
- **DH**: 2048-bit FFDHE (SP math)
- **ECC**: P-256, P-384 (ECDHE, ECDSA)
- **Curve25519 / Ed25519, Curve448 / Ed448**
- **Post-quantum**: ML-KEM 512/768/1024, ML-DSA 44/65/87

SHA-1, MD5, DES3 and DSA are disabled.

## Running

Connect to the USB-C port at 115200 8N1. Every run clears the terminal and
prints a banner, followed by the benchmark:

```
wolfSSL STM32U585 @ 160 MHz, hardware accelerated
wolfSSL 5.9.4 (CONFIG=hw), benchmark run 1
setting stack relative offset reference mark in benchmark_test to +112
wolfCrypt Benchmark (block bytes 1024, min 1.0 sec each)
AES-128-CBC-enc              9 MiB took 1.002 seconds,    9.137 MiB/s [heap 0 bytes (0 allocs), stack 368 bytes]
...
Benchmark complete
total   Allocs   =      2901
total   Deallocs =      2901
total   Bytes    =  21148992
peak    Bytes    =    123072
current Bytes    =         0
```

The memory summary at the end comes from wolfSSL's memory tracker, which
also provides the per-line heap figures.

To show it as a live table, see [bench_tui.py](../README.md#bench_tuipy).

Hold **BT2** while pressing RESET (or powering on) to run an LED-only demo
instead, which cross-fades the LED bars.

### Flashing over UART

The STM32 ROM bootloader can be used instead of a debug probe. Set the BOOT0
switch to "PROG", press RESET and flash `build/bench-hw/app.hex` with
STM32CubeProgrammer or `stm32flash`:

```bash
stm32flash /dev/ttyUSB0 -b115200 -w build/bench-hw/app.hex
```

Then set the switch back to "RUN" and press RESET.

## Example Results

wolfSSL 5.9.4 at 160 MHz:

| Algorithm | `hw` | `c` | Unit |
|-----------|-----:|----:|------|
| AES-128-CBC encrypt | 9.137 | 2.303 | MiB/s |
| AES-128-GCM encrypt | 8.406 | 1.247 | MiB/s |
| AES-256-GCM encrypt | 7.480 | 1.056 | MiB/s |
| SHA-256 | 17.097 | 3.338 | MiB/s |
| HMAC-SHA256 | 17.114 | 3.310 | MiB/s |
| RSA 2048 public | 211.366 | 72.266 | ops/s |
| RSA 2048 private | 5.655 | 1.290 | ops/s |
| DH 2048 agree | 14.363 | 3.120 | ops/s |
| ECDHE P-256 agree | 112.000 | 41.874 | ops/s |
| ECDSA P-256 sign | 55.227 | 53.785 | ops/s |
| ECDSA P-256 verify | 52.632 | 37.698 | ops/s |
| ECDSA P-384 sign | 21.422 | 20.276 | ops/s |
| ECDSA P-384 verify | 19.212 | 13.605 | ops/s |

## Directory Layout

```
wolfCrypt-bench/
|-- Makefile                 # Build system (TARGET / CONFIG variants)
|-- user_settings.h          # wolfSSL configuration
|-- README.md
|-- .gitignore
|-- src/
|   |-- main_bench.c         # wolfCrypt benchmark entry (loops forever)
|   |-- main_test.c          # wolfCrypt self-test entry
|   |-- board.h              # Board support API
|   |-- hw_init.c            # Clock, GPIO, UART, printf retarget, crypto MSP
|   |-- leds.c               # LED bar stepping and the BT2 LED demo
|   |-- stubs.c              # _sbrk, time() and file syscall stubs
|   |-- stm32u5xx_hal_conf.h # HAL module selection
|   |-- stm32u5xx_it.c       # Interrupt handlers (SysTick)
|   |-- system_stm32u5xx.c   # CMSIS SystemInit
|   `-- startup_stm32u585xx.s # Vector table + reset handler
`-- linker/
    `-- stm32u585ci_flat.ld  # 2 MB flash, 768 KB SRAM, 128 KB stack
```

## Make Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `WOLFSSL_ROOT` | `../../../../wolfssl` | wolfSSL source tree |
| `STM32CUBE_FW_U5` | `~/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0` | ST STM32CubeU5 package |
| `CONFIG` | `hw` | Build variant: `hw` or `c` |
| `TARGET` | `bench` | Entry point: `bench` or `test` |
| `OPENOCD_INTERFACE` | `stlink` | OpenOCD probe for `make flash`: `stlink` or `jlink` |

## Notes

- The wolfSSL STM32 port (`wolfcrypt/src/port/st/stm32.c`) is used through the
  STM32Cube HAL (`WOLFSSL_STM32_CUBEMX`). The application provides the PKA
  handle (`hpka` in `src/hw_init.c`).
- The PKA does not finish initialising unless the RNG peripheral is clocked,
  so the RNG clock is left running.
- On the STM32U5 the PKA signs at about the same speed as the pure C SP math
  (`CONFIG=c`), and verifies about 40% faster. ECC key generation and ECDHE
  use the SP math assembly.
- ML-DSA signing uses rejection sampling, so its result varies from run to
  run.
- wolfSSL 5.9.x only benchmarks the DRBG ("RNG" row) when there is a software
  seed source, so that row is not shown when seeding from the TRNG.
