# STM32C5A3_Nucleo_OTA

Secure over-the-air firmware update on the **NUCLEO-C5A3ZG** (STM32C5A3, Cortex-M33, 1 MB flash, 256 KB SRAM) using three wolfSSL products together:

- **wolfBoot** verifies every image with ML-DSA-44 (optionally ML-DSA-44 + ECDSA P-256 hybrid), refuses tampered images and downgrades, and resumes a swap interrupted by a power cut.
- **wolfIP** provides the Ethernet TCP/IP stack (DHCP with a static fallback).
- **wolfSSL** downloads the update over TLS 1.3 with a hybrid post-quantum key exchange (X25519 + ML-KEM-768 by default) and ECDSA P-256 mutual authentication, using the C5 hardware AES-GCM, SHA-2, PKA and TRNG.

The application runs in the wolfBoot BOOT partition. It asks the host OTA server for a newer version, streams the signed image into the UPDATE partition, and triggers wolfBoot, which verifies the image before swapping it in. The new firmware confirms itself with `wolfBoot_success()`.

## Requirements

- NUCLEO-C5A3ZG with Ethernet to the same network as the host
- `arm-none-eabi-gcc` and `STM32_Programmer_CLI`
- Sibling checkouts of [wolfSSL](https://github.com/wolfSSL/wolfssl), [wolfIP](https://github.com/wolfSSL/wolfip) and [wolfBoot](https://github.com/wolfSSL/wolfboot) (override with `WOLFSSL_ROOT`, `WOLFIP_ROOT`, `WOLFBOOT_ROOT`). wolfSSL must include the STM32C5 hardware GCM and SHA-2 support (wolfSSL PR 12144).
- STM32C5 CMSIS device headers (`CUBEC5_INC`) and CMSIS Core headers (`CMSIS_CORE`); see the defaults at the top of the `Makefile`
- A host wolfSSL install with TLS 1.3 and ML-KEM for the OTA server (`WOLFSSL_PREFIX`, default `/usr/local`)

## Flash layout

| Region | Address | Size |
|---|---|---|
| wolfBoot | `0x08000000` | 64 KB |
| BOOT partition (running image) | `0x08010000` | 448 KB |
| UPDATE partition | `0x08080000` | 448 KB |
| SWAP sector | `0x080F0000` | 8 KB |

The application links at `0x08012000`, behind the 8 KB wolfBoot image header.

## Build and sign

```sh
OTA_SERVER_IP=<host address> ./demo/prepare.sh
```

`prepare.sh` configures and builds wolfBoot from `config/examples/stm32c5.config` with ML-DSA-44, builds the application, signs it as versions 1, 2 and 3, makes a tampered copy of version 3 (one payload byte flipped after signing), and writes the flash commands to `demo/out/flash_cmds.txt`.

Options:

- `HYBRID=1` signs with ML-DSA-44 + ECDSA P-256
- `BENCH=1` runs the TLS 1.3 handshake benchmark at boot
- `OTA_GROUP=WOLFSSL_SECP256R1MLKEM768` selects another key share for the update check
- `STLINK_SN=<serial>` pins the probe in the generated flash commands

The demo PKI is in `certs/`; `certs/gen_certs.sh` regenerates it and `include/certs.h`. These keys are for the demo only.

## Run

```sh
make -C host WOLFSSL_PREFIX=<host wolfSSL install>
./host/ota_server -c certs -f demo/out/app_v2_signed.bin
```

Flash wolfBoot and version 1 with the first command in `demo/out/flash_cmds.txt`. The board boots version 1, gets an address over DHCP, finds version 2 on the server, downloads it, and reboots into it.

The console is USART2 on the ST-LINK virtual COM port at 115200 8N1:

| Key | Action |
|---|---|
| `u` or B1 | Check the server for an update |
| `b` | Benchmark the TLS 1.3 handshake for every key-exchange group |
| `t` | Install an image already staged in UPDATE (for example written over SWD) |

The server re-reads the image file on every request, so the served version can be changed between checks.

## Scenarios

Two scripts drive the full sequence. They read the console through [uart-monitor](https://github.com/dgarske/uart-monitor):

- `demo/net_scenarios.sh <ST-LINK SN> <uart-monitor label> <served file> <log dir>`: v1 updates to v2 at boot, a tampered v3 is rejected, v3 is installed, and a downgrade to v1 is refused, all over the network.
- `demo/swd_scenarios.sh <ST-LINK SN> <uart-monitor label> <log dir>`: the same checks with images staged over SWD, plus a power cut in the middle of a swap that wolfBoot resumes on the next boot. Stop the OTA server first, or the board updates itself over the network at boot.

The STM32C5 copy-based update expects the `SWAP_BANK` option byte to be 0; both scripts reset it if a dual-bank test left it set.
