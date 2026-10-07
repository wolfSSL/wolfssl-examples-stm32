# ---------------------------------------------------------------------------
# STM32_Bare_Test - HAL-free wolfCrypt validation across STM32 boards
#
# Boards (BOARD=):
#   h5    - NUCLEO-H563ZI    (HASH + RNG only; no AES on this chip)
#   f437  - STM32437I-EVAL   (CRYP + HASH + RNG)
#                              UART4 PC10/PC11 AF8 VCP; 144 MHz HSI->PLL.
#                              Build-validated; silicon-on-bench pending board.
#
# Build flavors (CONFIG=):
#   c     - Pure C software baseline (no STM32 HW)
#   bare  - WOLFSSL_STM32_BARE: direct-register HW (no HAL, no StdPeriph)
#
# Targets (TARGET=):
#   test  - wolfCrypt KAT/self-test
#   bench - wolfCrypt benchmark
#   dhuk  - DHUK ECC sign + GMAC tests (u3/u585/u545 only)
#
# Examples:
#   make BOARD=h5  CONFIG=bare TARGET=test
#   make BOARD=h5  CONFIG=bare TARGET=bench
#   make BOARD=h5  CONFIG=c    TARGET=test
#   make flash    BOARD=h5  CONFIG=bare TARGET=test
#   make BOARD=u3  CONFIG=bare TARGET=dhuk flash
#
# Required env / make vars:
#   WOLFSSL_ROOT     = ../../wolfssl                  (default sibling layout)
#   STM32CUBE_FW_H5  = $(HOME)/STM32Cube/Repository/STM32Cube_FW_H5_V1.5.1
#   STM32CUBE_FW_F4  = $(HOME)/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3
# ---------------------------------------------------------------------------

CROSS    ?= arm-none-eabi-
CC        = $(CROSS)gcc
AS        = $(CROSS)gcc -x assembler-with-cpp
OBJCOPY   = $(CROSS)objcopy
SIZE      = $(CROSS)size

TOP          := $(abspath .)
REPO_SIBLING := $(abspath $(TOP)/../..)
WOLFSSL_ROOT ?= $(REPO_SIBLING)/wolfssl

BOARD    ?= h5
CONFIG   ?= bare
TARGET   ?= test
BUILD    ?= bare
PQC      ?= 0
MLDSA_SET ?= all
STACK    ?= 0

ifeq ($(filter $(BOARD),c031 h5 h573 h7 h7a3 h7s3 h723 u5 u3 u585 u545 u083 f207 f303 f437 f439 f767 wb55 wl55 g071 g474 g491 wba52 n657 c562 c5a3 l4a6 l552 l562 v8),)
  $(error BOARD must be one of: c031 | h5 | h573 | h7 | h7a3 | h7s3 | h723 | u5 | u3 | u585 | u545 | u083 | f207 | f303 | f437 | f439 | f767 | wb55 | wl55 | g071 | g474 | g491 | wba52 | n657 | c562 | c5a3 | l4a6 | l552 | l562 | v8)
endif

ifeq ($(filter $(CONFIG),c asm bare),)
  $(error CONFIG must be one of: c | asm | bare)
endif
ifeq ($(filter $(TARGET),test bench dhuk mtls stsaes ccb ccbhal c5rng c5sign cbonly puf cubeaes cubecrypto aesplain plaingcm gcmalign v8sha3),)
  $(error TARGET must be one of: test | bench | dhuk | mtls | stsaes | ccb | ccbhal | c5rng | c5sign | cbonly | puf | cubeaes | cubecrypto | aesplain | plaingcm | gcmalign | v8sha3)
endif
# c5rng: bare register-level STM32C5 HW-RNG conditioning probe (no wolfCrypt).
# c5sign: bare STM32C5 PROTECTED ECDSA-sign probe driving ST's NIST P-256 CAVP
# vector through the PKA registers directly, compared to ST's expected R,S.
# Runs ST's CCB_RNG_Init / CONDRST + NIST sequence on live silicon and reports
# the precise failure mode. C5 boards only.
# v8sha3: bare register-level STM32V8 HASH SHA-3/SHAKE probe (no wolfCrypt).
ifeq ($(TARGET),v8sha3)
  ifneq ($(BOARD),v8)
    $(error TARGET=v8sha3 requires BOARD=v8)
  endif
endif
ifneq ($(filter $(TARGET),c5rng c5sign),)
  ifeq ($(filter $(BOARD),c5a3 c562),)
    $(error TARGET=$(TARGET) requires a C5 board: c5a3 | c562)
  endif
endif
# stsaes: run ST's HAL SAES wrapped-key (DHUK) flow directly. Needs the
# CubeMX HAL (HAL_CRYPEx_WrapKey/UnwrapKey), so force BUILD=cubemx.
ifeq ($(TARGET),stsaes)
  ifneq ($(BUILD),cubemx)
    $(error TARGET=stsaes requires BUILD=cubemx)
  endif
endif
# ccbhal: ST HAL CCB ECDSA reference (blob create -> public key -> sign) on the
# U385. Used to confirm the CCB flow works on silicon and as a known-good
# reference for the bare WOLFSSL_STM32_CCB port. The HAL CCB driver only exists
# for U3, so force BUILD=cubemx and BOARD=u3.
ifeq ($(TARGET),ccbhal)
  ifneq ($(BUILD),cubemx)
    $(error TARGET=ccbhal requires BUILD=cubemx)
  endif
  ifeq ($(filter $(BOARD),u3),)
    $(error TARGET=ccbhal requires a CCB board: u3 (NUCLEO-U385RG-Q))
  endif
endif
# BUILD axis: bare = direct-register board init + WOLFSSL_STM32_BARE
#             cubemx = CubeMX/HAL board init + WOLFSSL_STM32_CUBEMX
# The CONFIG axis is orthogonal (bare/asm/c controls the crypto path;
# BUILD controls the board-bring-up + HAL-driver link surface).
ifeq ($(filter $(BUILD),bare cubemx),)
  $(error BUILD must be one of: bare | cubemx)
endif
# PQC axis: 0 = none, 1 = ML-DSA + ML-KEM, mldsa* = ML-DSA only, with the
# small-memory variants (matches the wolfBoot resource-constrained
# preset). Adds substantial code so small-flash boards (c031, g071,
# u083) may overflow.
PQC_VALUES := 0 1 mldsa mldsa-smallest mldsa-vfy mldsa-vfy-pin mldsa-vfy-fast
ifeq ($(filter $(PQC),$(PQC_VALUES)),)
  $(error PQC must be one of: $(PQC_VALUES))
endif
# MLDSA_SET axis: which ML-DSA parameter sets to compile in. all = every set
# the PQC variant allows; 44 | 65 | 87 = that set alone, for per-set code
# size and for giving one set the whole RAM budget.
ifeq ($(filter $(MLDSA_SET),all 44 65 87),)
  $(error MLDSA_SET must be one of: all 44 65 87)
endif
# STACK axis: 0 = bench prints perf only (default; byte-identical to today).
#             1 = bench prints per-algorithm peak stack + peak heap +
#                 heap allocs alongside perf. Enables wolfssl's
#                 HAVE_STACK_SIZE_VERBOSE + WOLFSSL_TRACK_MEMORY_VERBOSE +
#                 USE_WOLFSSL_MEMORY. Adds a small RAM + code overhead.
ifeq ($(filter $(STACK),0 1),)
  $(error STACK must be 0 or 1)
endif

BUILD_DIR := $(TOP)/build/$(BOARD)-$(BUILD)-$(TARGET)-$(CONFIG)

# ---- Per-board settings ----------------------------------------------------
# Each board's config lives in boards/<BOARD>/board.mk (raw atoms; multi-
# board families share boards/families/<fam>/family.mk). board-derive.mk
# then computes the mechanical vars (MCU_FLAGS, CMSIS dirs, CPU_DEFS,
# LDSCRIPT, STARTUP_S, BOARD_C_SRC, ...). A board.mk may set any of those
# directly to override the derivation.
include $(TOP)/boards/$(BOARD)/board.mk
include $(TOP)/boards/common/board-derive.mk

# ---- Variant overlays ------------------------------------------------------
ifeq ($(CONFIG),bare)
  VARIANT_DEFS := -DBUILD_BARE
endif
ifeq ($(CONFIG),c)
  VARIANT_DEFS := -DBUILD_C
endif
ifeq ($(CONFIG),asm)
  VARIANT_DEFS := -DBUILD_ASM
endif

# ---- BUILD axis: BARE vs CubeMX HAL ----------------------------------------
# BUILD=bare keeps wolfcrypt on the direct-register path (WOLFSSL_STM32_BARE).
# BUILD=cubemx pulls in ST's HAL drivers for clock/UART/GPIO + the crypto
# HAL modules wolfcrypt's WOLFSSL_STM32_CUBEMX path drives (CRYP/HASH/RNG/
# PKA). The wolfcrypt port_st_stm32.c code already handles both, so the only
# Makefile delta is the HAL source-list + include path + the chip's
# stm32<f>xx_hal_conf.h header.
ifeq ($(BUILD),cubemx)
  ifeq ($(HAL_FAMILY),)
    $(error BUILD=cubemx not yet wired for BOARD=$(BOARD); needs HAL_FAMILY \
            and boards/$(BOARD)/hw_init_cubemx.c. Currently supported: h7)
  endif
  BUILD_DEFS    := -DSTM32_BUILD_CUBEMX -DUSE_HAL_DRIVER
ifeq ($(b_hal_newgen),1)
  # New-generation HAL ("HAL2", e.g. STM32C5). The DFP layout and module set
  # differ from the classic HAL, and the new-gen HAL has no classic crypto
  # driver APIs -- so wolfcrypt stays on the register path (WOLFSSL_STM32_BARE,
  # selected in user_settings.h via STM32_HAL_NEWGEN) and these HAL modules only
  # bring up the board (clock/cache/NVIC). board.mk supplies HAL_SRC_DIR,
  # HAL_INC_DIR, HAL_C_SRC (board-init modules + generated mx_*.c) and any
  # HAL_EXTRA_INC (ll / timebases / generated headers).
  HAL_C_SRC := $(wildcard $(HAL_C_SRC))
else
  HAL_DRIVER_DIR := $(CUBE_DIR)/Drivers/STM32$(HAL_FAMILY)_HAL_Driver
  HAL_INC_DIR    := $(HAL_DRIVER_DIR)/Inc
  HAL_SRC_DIR    := $(HAL_DRIVER_DIR)/Src
  # Minimum HAL modules to bring up the board + crypto. Avoid pulling in
  # peripherals we don't use (eth/dma2d/i2c/spi/...) to keep build small.
  HAL_C_SRC := \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_cortex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_rcc.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_rcc_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_pwr.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_pwr_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_gpio.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_uart.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_uart_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_dma.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_dma_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_flash.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_flash_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_cryp.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_cryp_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_hash.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_hash_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_rng.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_rng_ex.c \
    $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_pka.c
  # CCB targets (U3 only) need the CCB HAL driver module: ccbhal is the raw HAL
  # reference; ccb / cubecrypto under cubemx route the wolfSSL port through HAL_CCB.
  ifneq ($(filter $(TARGET),ccb ccbhal cubecrypto),)
    HAL_C_SRC += $(HAL_SRC_DIR)/stm32$(HAL_FAMILY_LC)_hal_ccb.c
  endif
  # Filter out any files that don't exist (different family packs ship
  # different module subsets; e.g. STM32H7 has hash_ex but no pka, while
  # STM32U5 has pka but not all the same ex modules).
  HAL_C_SRC := $(wildcard $(HAL_C_SRC))
endif
else
  BUILD_DEFS := -DSTM32_BUILD_BARE
  HAL_C_SRC  :=
endif

# ---- Include paths ---------------------------------------------------------
INCLUDES := \
  -I$(TOP) \
  -I$(TOP)/include \
  -I$(TOP)/src \
  -I$(CMSIS_CORE) \
  -I$(CMSIS_DEVICE) \
  -I$(WOLFSSL_ROOT)
ifeq ($(BUILD),cubemx)
  INCLUDES += -I$(HAL_INC_DIR) -I$(HAL_CONF_DIR) $(HAL_EXTRA_INC)
endif

# ---- Compile / link flags --------------------------------------------------
# PQC=1 enables ML-DSA + ML-KEM with the small-memory variants. The
# actual #defines (HAVE_DILITHIUM, WOLFSSL_DILITHIUM_*, WOLFSSL_HAVE_MLKEM,
# WOLFSSL_MLKEM_*_SMALL_MEM) live in user_settings.h gated on
# STM32_BARE_PQC.
ifeq ($(PQC),1)
  PQC_DEFS := -DSTM32_BARE_PQC
else ifeq ($(PQC),mldsa)
  # Key generation + sign + verify, small-memory variants. No ML-KEM, which
  # is what lets this fit the 128 KB small-flash boards that PQC=1 overflows.
  PQC_DEFS := -DSTM32_BARE_PQC_MLDSA
else ifeq ($(PQC),mldsa-smallest)
  # As above with the smallest-memory signer: one polynomial of y instead of
  # the vector, regenerated for z.
  PQC_DEFS := -DSTM32_BARE_PQC_MLDSA -DSTM32_BARE_PQC_MLDSA_SIGN_SMALLEST
else ifeq ($(PQC),mldsa-vfy)
  # Verify only, smallest-memory verify with allocated buffers.
  PQC_DEFS := -DSTM32_BARE_PQC_MLDSA -DSTM32_BARE_PQC_MLDSA_VFY
else ifeq ($(PQC),mldsa-vfy-pin)
  # Verify only, smallest-memory verify with the buffers pinned in the key:
  # no allocator at all, at a larger key.
  PQC_DEFS := -DSTM32_BARE_PQC_MLDSA -DSTM32_BARE_PQC_MLDSA_VFY \
              -DSTM32_BARE_PQC_MLDSA_VFY_PIN
else ifeq ($(PQC),mldsa-vfy-fast)
  # Verify only, small-memory verify: faster at a higher peak, which is the
  # better trade whenever the RAM is there.
  PQC_DEFS := -DSTM32_BARE_PQC_MLDSA -DSTM32_BARE_PQC_MLDSA_VFY \
              -DSTM32_BARE_PQC_MLDSA_FAST
else
  PQC_DEFS :=
endif
ifneq ($(MLDSA_SET),all)
  PQC_DEFS += -DSTM32_BARE_PQC_MLDSA_SET=$(MLDSA_SET)
endif
ifeq ($(SHA3_ASM),1)
  SHA3_DEFS := -DSTM32_BARE_SHA3_ASM
else
  SHA3_DEFS :=
endif

# STACK=1 enables per-algorithm peak stack + peak heap reporting. The
# actual #defines (USE_WOLFSSL_MEMORY, WOLFSSL_TRACK_MEMORY_VERBOSE,
# HAVE_STACK_SIZE_VERBOSE) live in user_settings.h gated on
# STM32_BARE_STACK_TRACK.
ifeq ($(STACK),1)
  STACK_DEFS := -DSTM32_BARE_STACK_TRACK
else
  STACK_DEFS :=
endif

# GCM=small selects the GCM_SMALL multiply (via gcm->H directly) instead of
# the default GCM_TABLE_4BIT (via the gcm->M0 table). user_settings.h gates
# the GCM impl on STM32_BARE_GCM_SMALL. Used by the DHUK GMAC cross-config
# KAT: a correct table build must produce the same tag as the small build on
# the same device key, which pins the GenerateM0() wiring in Stm32Dhuk_Gmac.
ifeq ($(GCM),small)
  GCM_DEFS := -DSTM32_BARE_GCM_SMALL
else ifeq ($(GCM),table)
  GCM_DEFS := -DSTM32_BARE_GCM_TABLE
else
  GCM_DEFS :=
endif

# PUF axis: TARGET=puf builds the SRAM PUF regression (src/main_puf.c),
# gating WOLFSSL_PUF + WOLFSSL_PUF_TEST in user_settings.h. PUF_T selects
# the BCH profile (7|10|13|15); PUF_CW selects WC_PUF_NUM_CODEWORDS.
ifeq ($(TARGET),puf)
  PUF_DEFS := -DSTM32_BARE_PUF
  ifneq ($(PUF_T),)
    ifeq ($(filter $(PUF_T),7 10 13 15),)
      $(error PUF_T must be one of 7, 10, 13, 15)
    endif
    PUF_DEFS += -DWC_PUF_BCH_T=$(PUF_T)
  endif
  ifneq ($(PUF_CW),)
    PUF_DEFS += -DWC_PUF_NUM_CODEWORDS=$(PUF_CW)
  endif
  # PUF_COMPACT=1 selects the opt-in parity-only helper layout.
  ifeq ($(PUF_COMPACT),1)
    PUF_DEFS += -DWC_PUF_HELPER_COMPACT
  endif
else
  PUF_DEFS :=
endif

# STACK_SZ overrides the linker's stack carve, as a decimal byte count (no
# K/M suffix). Every board's linker script honours _stack_size, so this works
# on any BOARD. The carve sets _heap_limit, the static ceiling _sbrk will not
# grow the heap past; src/stubs.c additionally refuses to grow into the live
# stack, so an under-sized carve surfaces as an allocation failure rather than
# heap/stack corruption.
ifneq ($(STACK_SZ),)
  ifneq ($(shell echo '$(STACK_SZ)' | grep -cE '^[0-9]+$$'),1)
    $(error STACK_SZ must be a decimal byte count, got '$(STACK_SZ)')
  endif
  LD_EXTRA := -Wl,--defsym=_stack_size=$(STACK_SZ)
else
  LD_EXTRA :=
endif

# CLK selects a non-default SYSCLK where the board supports it. Only g071
# implements it so far: CLK=64 brings up the PLL for 64 MHz instead of
# running from HSI16. The board's clock_init() reads STM32_BARE_CLK_HZ.
ifneq ($(CLK),)
  ifeq ($(filter $(CLK),16 64),)
    $(error CLK must be one of: 16 | 64 (MHz))
  endif
  ifneq ($(BOARD),g071)
    $(error CLK= is only implemented for BOARD=g071)
  endif
  # Only the bare path brings up the PLL. boards/g071/hw_init_cubemx.c is
  # fixed at 16 MHz, so accepting CLK=64 there would run the benchmark at
  # 16 MHz while reporting 64.
  ifeq ($(BUILD),cubemx)
    ifneq ($(CLK),16)
      $(error CLK=$(CLK) needs BUILD=bare; the g071 CubeMX path is fixed at 16 MHz)
    endif
  endif
  CLK_DEFS := -DSTM32_BARE_CLK_HZ=$(CLK)000000
else
  CLK_DEFS :=
endif

# EXTRA_DEFS appends arbitrary -D flags, for a one-off measurement that
# does not warrant an axis (e.g. -DBENCH_MIN_RUNTIME_SEC=10.0 to average a
# rejection-sampled operation over more iterations).
EXTRA_DEFS ?=
COMMON_DEFS := -DWOLFSSL_USER_SETTINGS $(CPU_DEFS) $(VARIANT_DEFS) $(BUILD_DEFS) $(PQC_DEFS) $(SHA3_DEFS) $(STACK_DEFS) $(GCM_DEFS) $(PUF_DEFS) $(CLK_DEFS) $(EXTRA_DEFS)

# Optimization level, overridable for miscompile/UB bisects: make OPT=-O1 ...
OPT ?= -O2
CFLAGS    = $(MCU_FLAGS) $(INCLUDES) $(COMMON_DEFS) \
            $(OPT) -fomit-frame-pointer \
            -ffunction-sections -fdata-sections \
            -Wall -Wno-unused-function -Wno-unused-variable \
            -fno-common -fno-strict-aliasing \
            -MMD -MP -std=gnu11

ASFLAGS  := $(MCU_FLAGS) -c -g3

# C031 has 32 KB flash -- float printf pulls in ~6 KB of FP support
# from newlib-nano that we can't afford. Other boards keep it for
# benchmark percent formatting.
ifeq ($(BOARD),c031)
  PRINTF_FLOAT_FLAG :=
else
  PRINTF_FLOAT_FLAG := -u _printf_float
endif

LDFLAGS  := $(MCU_FLAGS) \
            $(LD_EXTRA) \
            -T$(LDSCRIPT) \
            -Wl,-L,$(TOP)/boards/common \
            --specs=nosys.specs --specs=nano.specs \
            $(PRINTF_FLOAT_FLAG) \
            -Wl,--gc-sections \
            -Wl,-Map=$(BUILD_DIR)/app.map \
            -Wl,--print-memory-usage

LDLIBS   := -lc -lm

# ---- Sources ---------------------------------------------------------------
SRC_C  := \
  src/main_$(TARGET).c \
  src/stubs.c \
  boards/common/board_common.c \
  $(BOARD_C_SRC) \
  $(HAL_C_SRC)

SRC_ASM := $(STARTUP_S)

# wolfCrypt -- compile the whole wolfcrypt/src tree; user_settings gates
# what isn't needed. No HAL drivers under any CONFIG here -- BARE goes
# direct-register; C is software-only.
WC_SRC_DIR := $(WOLFSSL_ROOT)/wolfcrypt/src
WC_ALL     := $(wildcard $(WC_SRC_DIR)/*.c)
WC_EXCLUDE := \
  $(WC_SRC_DIR)/evp.c \
  $(WC_SRC_DIR)/misc.c \
  $(WC_SRC_DIR)/selftest.c \
  $(WC_SRC_DIR)/async.c \
  $(WC_SRC_DIR)/wolfcrypt_first.c \
  $(WC_SRC_DIR)/wolfcrypt_last.c \
  $(WC_SRC_DIR)/fips.c \
  $(WC_SRC_DIR)/fips_test.c

WC_SRC := $(filter-out $(WC_EXCLUDE),$(WC_ALL)) \
          $(WC_SRC_DIR)/port/st/stm32.c

# Thumb2 inline-assembly ports (built unconditionally; macro-gated inside).
# CONFIG=asm activates them via WOLFSSL_ARMASM in user_settings.h.
WC_SRC += \
  $(WC_SRC_DIR)/port/arm/thumb2-chacha-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-poly1305-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-curve25519_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-sha512-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-aes-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-sha256-asm_c.c

# MLKEM thumb2 ASM is only referenced by wc_mlkem_poly.c when
# WOLFSSL_ARMASM is active AND ML-KEM is enabled. PQC=1 enables ML-KEM,
# so wire it in only there. (thumb2-sha3-asm_c.c also exists upstream
# but adding it unconditionally pushes wl55 over its 256 KB flash, so
# SHA3 stays on the C path via WC_SHA3_NO_ASM in user_settings.h.
# SHA3_ASM=1 opts in on large-flash boards: 2.5x SHA-3/SHAKE and 2x
# ML-DSA sign measured on the 4 MB v8.)
ifeq ($(PQC),1)
  WC_SRC += $(WC_SRC_DIR)/port/arm/thumb2-mlkem-asm_c.c
endif
ifeq ($(SHA3_ASM),1)
  WC_SRC += $(WC_SRC_DIR)/port/arm/thumb2-sha3-asm_c.c
endif

# Target entry point
ifeq ($(TARGET),test)
  # C031C6 has only 32 KB flash -- the wolfcrypt_test suite alone is
  # ~30+ KB even with NO_RSA/NO_ECC/NO_DH. Skip test.c entirely on C031
  # and rely on the main_test.c KAT block (SHA-256 + AES-CBC/ECB) as the
  # validation surface.
  ifneq ($(BOARD),c031)
    SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/test/test.c
    COMMON_DEFS += -DRUN_WOLFCRYPT_TEST_SUITE
  endif
endif
ifeq ($(TARGET),bench)
  SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/benchmark/benchmark.c
endif

# TLS 1.3 mutual auth with a DHUK-wrapped client key, client and server both
# in this image over an in-memory transport. Same SAES + DHUK + PKA
# requirement as the dhuk target, so the same board set. The src/ssl_*.c,
# x509*.c, pk*.c, bio.c and conf.c files are #included into ssl.c and must not
# be compiled standalone.
ifeq ($(TARGET),mtls)
  # Needs room for TLS 1.3 + ECC + the cert buffers (~250 KB flash, ~165 KB
  # RAM), which rules out the small-memory boards in this matrix.
  ifneq ($(filter $(BOARD),c031 g071 u083 wl55 f303 g474 g491 h7s3),)
    $(error TARGET=mtls needs more flash/RAM than BOARD=$(BOARD) has)
  endif
  COMMON_DEFS += -DSTM32_BARE_MTLS
  # SAES + DHUK + PKA boards additionally run the client key as a
  # device-wrapped blob; everywhere else the same handshake uses an ordinary
  # key, so the target is a plain TLS 1.3 mutual-auth regression test.
  ifneq ($(filter $(BOARD),u3 u585 u545),)
    # Only CONFIG=bare enables the PKA the wrapped key needs; any other config
    # would quietly build the ordinary-key path on a device-key board.
    ifneq ($(CONFIG),bare)
      $(error TARGET=mtls on BOARD=$(BOARD) requires CONFIG=bare)
    endif
    COMMON_DEFS += -DWOLFSSL_DHUK -DWOLF_CRYPTO_CB
  endif
  WC_SRC += $(addprefix $(WOLFSSL_ROOT)/src/, \
              ssl.c internal.c tls.c tls13.c keys.c wolfio.c \
              crl.c ocsp.c dtls.c dtls13.c quic.c)
endif
# DHUK crypto-callback test app (GMAC + AES-ECB + ECDSA sign). The ECC
# sign route requires SAES + DHUK + PKA together, so this target is
# limited to the boards that carry all three (u3 = U385, u585 = U585,
# u545 = U545). Other DHUK-only or PKA-less parts (e.g. u5 = U575, which
# has neither SAES nor PKA) would fail to link. -DWOLFSSL_DHUK plus
# -DWOLF_CRYPTO_CB enable the vendor-neutral DHUK device; the optional
# exact-key unwrap primitive stays gated behind -DWOLFSSL_STM32_DHUK_UNWRAP.
ifeq ($(TARGET),dhuk)
  # c562 is excluded: the STM32C562 device header in the C5 DFP defines no
  # SAES (and no AES_CR_KEYSEL_0) at all, unlike stm32c5a3xx.h, so every
  # DHUK target fails to compile there. Re-add once a DFP with SAES ships.
  ifeq ($(filter $(BOARD),u3 u585 u545 c5a3),)
    $(error TARGET=dhuk requires a SAES+DHUK board: u3 u585 u545 c5a3)
  endif
  COMMON_DEFS += -DWOLFSSL_DHUK
  # DHUK routes through the wolfCrypt crypto-callback framework
  # (the STM32 cryptocb device, wc_Stm32_DhukRegister), so WOLF_CRYPTO_CB is
  # required.
  COMMON_DEFS += -DWOLF_CRYPTO_CB
  # DHUK_UNWRAP=1 opts into the real SAES wrapped-key DECRYPT path (the
  # unwrap that lands a key in SAES KEYR). Verified from non-secure state
  # (TZEN=0) on u3, u585 and u545; secure state is not required. Without the
  # flag the default CRYPTOCB_UNAVAILABLE stubs are linked.
  ifeq ($(DHUK_UNWRAP),1)
    COMMON_DEFS += -DWOLFSSL_STM32_DHUK_UNWRAP
  endif
  # SECURE=1: build for a TZEN=1 part running in secure state. -mcmse
  # makes the CMSIS device header resolve every peripheral (PWR/RCC/
  # FLASH/GPIO/USART/SAES/PKA) to its secure alias (0x5xxx), which is
  # mandatory once TrustZone is enabled -- the non-secure aliases the
  # TZEN=0 build uses do not bring the clocks up from secure state.
  # Code/data keep their default aliases; the SAU stays disabled (reset
  # default = all-secure), so execution is already secure. Intended for
  # the B-U585I-IOT02A (flash secure-watermarked, TZEN=0x1).
  ifeq ($(SECURE),1)
    COMMON_DEFS += -mcmse
    LDFLAGS     += -mcmse
  endif
endif
# CCB (Coupling and Chaining Bridge) test app -- STM32U3 only (u3 = U385). Brings
# up the CCB and exercises the HW-protected DHUK->PKA ECDSA path; the CCB chains
# PKA <-> SAES <-> RNG so the private key never enters software. Needs SAES + PKA
# + DHUK + the CCB peripheral, which only the U3 carries. SECURE=1 builds for
# TZEN=1 secure state (the fallback if CCB will not advance from non-secure).
ifeq ($(TARGET),ccb)
  ifeq ($(filter $(BOARD),u3 c5a3),)
    $(error TARGET=ccb requires a CCB board: u3 (NUCLEO-U385RG-Q) or c5a3 (NUCLEO-C5A3ZG))
  endif
  COMMON_DEFS += -DWOLFSSL_DHUK -DWOLF_CRYPTO_CB -DWOLFSSL_STM32_CCB
  ifeq ($(SECURE),1)
    COMMON_DEFS += -mcmse
    LDFLAGS     += -mcmse
  endif
endif
# Crypto-callback-only test app -- exercises ECDSA sign/verify, full-payload
# AES-GCM, HMAC-SHA256 and TRNG on hardware through the wolfCrypt crypto-callback
# framework (the STM32 DHUK device), and turns on the STM32_BARE_CB_ONLY preset
# that strips the software crypto so flash shrinks. The ECDSA sign route needs
# SAES + DHUK + PKA together, so this is limited to the same boards as dhuk.
ifeq ($(TARGET),cbonly)
  # c562 is excluded: the STM32C562 device header in the C5 DFP defines no
  # SAES (and no AES_CR_KEYSEL_0) at all, unlike stm32c5a3xx.h, so every
  # DHUK target fails to compile there. Re-add once a DFP with SAES ships.
  ifeq ($(filter $(BOARD),u3 u585 u545 c5a3),)
    $(error TARGET=cbonly requires a SAES+DHUK board: u3 u585 u545 c5a3)
  endif
  COMMON_DEFS += -DWOLFSSL_DHUK -DWOLF_CRYPTO_CB -DSTM32_BARE_CB_ONLY
  ifeq ($(SECURE),1)
    COMMON_DEFS += -mcmse
    LDFLAGS     += -mcmse
  endif
endif
# Plaintext-key AES vs DHUK-seed AES on the bare build -- registers both the
# plaintext AES device (wc_Stm32_AesRegister, WOLFSSL_STM32_AES_DEVID) and the
# DHUK device (WC_DHUK_DEVID) at once and selects between them per Aes by devId.
# Reuses the STM32_BARE_CB_ONLY preset (WOLF_CRYPTO_CB_ONLY_AES + DHUK); needs a
# SAES+DHUK board for the DHUK half of the coexistence check.
ifeq ($(TARGET),aesplain)
  # c562 is excluded: the STM32C562 device header in the C5 DFP defines no
  # SAES (and no AES_CR_KEYSEL_0) at all, unlike stm32c5a3xx.h, so every
  # DHUK target fails to compile there. Re-add once a DFP with SAES ships.
  ifeq ($(filter $(BOARD),u3 u585 u545 c5a3),)
    $(error TARGET=aesplain requires a SAES+DHUK board: u3 u585 u545 c5a3)
  endif
  COMMON_DEFS += -DWOLFSSL_DHUK -DWOLF_CRYPTO_CB -DSTM32_BARE_CB_ONLY
  # SECURE=1: same TZEN=1 secure-state build as dhuk / ccb / cbonly (see the
  # comment in the dhuk block above). The DHUK half of the coexistence check is
  # the path that needs secure state on a TZEN=1 part (e.g. the B-U585I-IOT02A).
  ifeq ($(SECURE),1)
    COMMON_DEFS += -mcmse
    LDFLAGS     += -mcmse
  endif
endif
# Direct HW AES-GCM (plaintext key) check -- calls wc_Stm32_Aes_Gcm() with no SW
# fallback and verifies against a published whole-block vector. Confirms the bare
# HW GCM engine does encrypt + decrypt-verify. Needs a board whose silicon carries
# an AES IP with a HW GCM mode: the CRYP IP on f437/f439/h7, or the TinyAES IP
# (routed to SAES on h7s3/n657) with AES_CR_GCMPH on h7s3/u3/u585/u545/l4a6/
# l562/wba52/n657.
# Excluded on purpose: u5 (U575), f767 (F767ZI) and l552 (L552ZE) have no AES IP
# at all (NO_STM32_CRYPTO in user_settings.h). c5a3/c562 do have the IP, but the
# STM32C5 CMSIS names the GCM phase field AES_CR_CPHASE instead of AES_CR_GCMPH,
# and wolfSSL only abstracts that rename (WC_STM32_AES_CR_PHASE) on the DHUK/SAES
# path -- the plaintext-key wc_Stm32_Aes_Gcm() compiles to the
# CRYPTOCB_UNAVAILABLE stub there. Use TARGET=aesplain on C5 instead.
ifeq ($(TARGET),plaingcm)
  ifeq ($(filter $(BOARD),f437 f439 h7 h7s3 u3 u585 u545 l4a6 l562 wba52 n657),)
    $(error TARGET=plaingcm requires a board with STM32 AES-GCM HW: f437 f439 h7 h7s3 u3 u585 u545 l4a6 l562 wba52 n657)
  endif
endif
# CubeMX AES crypto-callback test -- verifies WOLF_CRYPTO_CB_ONLY_AES works on
# the HAL build. Registers wc_Stm32_AesRegister (the new CubeMX AES device
# in wolfcrypt/src/port/st/stm32.c) and runs AES-GCM KATs with a plaintext key:
# the device's AES-ECB handler keys GCM, then bulk GCM runs on the native HAL
# engine. Requires BUILD=cubemx (HAL crypto driver) and BOARD=u3 (NUCLEO-U385RG-Q).
ifeq ($(TARGET),cubeaes)
  ifneq ($(BUILD),cubemx)
    $(error TARGET=cubeaes requires BUILD=cubemx)
  endif
  # CONFIG=bare is what defines WOLFSSL_STM32_CUBEMX (see user_settings.h), which
  # in turn pulls the family HAL header the app needs. Without it the app drops to
  # its "not enabled" arm and wolfssl raises its own #error, so gate it here to
  # give a clear message instead.
  ifneq ($(CONFIG),bare)
    $(error TARGET=cubeaes requires CONFIG=bare)
  endif
  ifeq ($(filter $(BOARD),u3),)
    $(error TARGET=cubeaes requires BOARD=u3 (NUCLEO-U385RG-Q))
  endif
  COMMON_DEFS += -DWOLF_CRYPTO_CB -DSTM32_CUBE_AES_ONLY
endif
# CubeMX full HW-crypto callback test -- HW ECDSA sign+verify (normal key -> PKA),
# CCB-protected ECDSA (HAL_CCB), and AES-GCM, all through the crypto callback under
# WOLF_CRYPTO_CB_ONLY_ECC + WOLF_CRYPTO_CB_ONLY_AES. Exercises the CubeMX device's
# new ECDSA verify + normal-key sign handlers. Needs the HAL PKA + CCB drivers.
ifeq ($(TARGET),cubecrypto)
  ifneq ($(BUILD),cubemx)
    $(error TARGET=cubecrypto requires BUILD=cubemx)
  endif
  # As for cubeaes: CONFIG=bare is what defines WOLFSSL_STM32_CUBEMX, which the
  # ST HAL PKA handle (PKA_HandleTypeDef hpka) in main_cubecrypto.c depends on.
  ifneq ($(CONFIG),bare)
    $(error TARGET=cubecrypto requires CONFIG=bare)
  endif
  ifeq ($(filter $(BOARD),u3),)
    $(error TARGET=cubecrypto requires BOARD=u3 (NUCLEO-U385RG-Q))
  endif
  COMMON_DEFS += -DWOLFSSL_STM32_PKA -DWOLFSSL_DHUK -DWOLF_CRYPTO_CB \
                 -DWOLFSSL_STM32_CCB -DSTM32_CUBE_CRYPTO_ONLY
endif

ALL_C_SRC := $(SRC_C) $(WC_SRC)

# ---- Object files ----------------------------------------------------------
rel_obj = $(BUILD_DIR)/obj/$(subst /,__,$(patsubst /%,%,$(1))).o

C_OBJ   := $(foreach s,$(ALL_C_SRC),$(call rel_obj,$(s)))
ASM_OBJ := $(foreach s,$(SRC_ASM),$(call rel_obj,$(s)))
OBJS    := $(C_OBJ) $(ASM_OBJ)
DEPS    := $(OBJS:.o=.d)

# Make tracks source edits, not flag changes. PQC=, CLK=, STACK_SZ= and the
# other axes all land in one BUILD_DIR, so switching one between runs would
# otherwise reuse the objects and ELF built under the previous flags. Each
# stamp records the effective flags and is rewritten only when they differ:
# every object depends on the compile stamp and the link on the link stamp,
# so a changed axis rebuilds exactly what it affects without a clean.
CFLAGS_STAMP  := $(BUILD_DIR)/cflags.stamp
LDFLAGS_STAMP := $(BUILD_DIR)/ldflags.stamp
CFLAGS_NOW    := $(CFLAGS) $(ASFLAGS)
LDFLAGS_NOW   := $(LDFLAGS) $(LDLIBS)

# ---- Targets ---------------------------------------------------------------
.PHONY: all build flash size clean distclean size-report

all: build

build: $(BUILD_DIR)/app.bin $(BUILD_DIR)/app.hex size

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/obj

.PHONY: FORCE
$(CFLAGS_STAMP): FORCE | $(BUILD_DIR)
	@echo '$(CFLAGS_NOW)' | cmp -s - $@ || echo '$(CFLAGS_NOW)' > $@

$(LDFLAGS_STAMP): FORCE | $(BUILD_DIR)
	@echo '$(LDFLAGS_NOW)' | cmp -s - $@ || echo '$(LDFLAGS_NOW)' > $@

define COMPILE_C_TEMPLATE
$(call rel_obj,$(1)): $(1) $(CFLAGS_STAMP) | $(BUILD_DIR)
	@echo "  CC   $$<"
	@mkdir -p $$(@D)
	@$(CC) $(CFLAGS) -c $$< -o $$@
endef

define COMPILE_ASM_TEMPLATE
$(call rel_obj,$(1)): $(1) $(CFLAGS_STAMP) | $(BUILD_DIR)
	@echo "  AS   $$<"
	@mkdir -p $$(@D)
	@$(AS) $(ASFLAGS) -c $$< -o $$@
endef

$(foreach s,$(ALL_C_SRC),$(eval $(call COMPILE_C_TEMPLATE,$(s))))
$(foreach s,$(SRC_ASM),$(eval $(call COMPILE_ASM_TEMPLATE,$(s))))

$(BUILD_DIR)/app.elf: $(OBJS) $(LDSCRIPT) $(LDFLAGS_STAMP)
	@echo "  LD   $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/app.bin: $(BUILD_DIR)/app.elf
	@echo "  BIN  $@"
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/app.hex: $(BUILD_DIR)/app.elf
	@$(OBJCOPY) -O ihex $< $@

size: $(BUILD_DIR)/app.elf
	@echo
	@echo "----- size: $(BOARD)-$(TARGET)-$(CONFIG) -----"
	@$(SIZE) $<

ifeq ($(BOARD),n657)
# N657 has no internal flash. Load .bin to AXISRAM2 (0x34000000) via OpenOCD,
# extract initial SP/PC from the vector table, set MSP/MSPLIM/VTOR, resume.
N657_SRAM_ADDR := 0x34000000
flash: $(BUILD_DIR)/app.bin
	@INIT_SP=$$(od -A n -t x4 -N 4 $< | awk '{print "0x"$$1}'); \
	 ENTRY=$$(od -A n -j 4 -t x4 -N 4 $< | awk '{print "0x"$$1}'); \
	 ENTRY_T=$$(printf "0x%08x" $$(( $$ENTRY | 1 ))); \
	 echo "  N657 SRAM-load SP=$$INIT_SP entry=$$ENTRY_T"; \
	 openocd -f interface/stlink.cfg \
	     -c "adapter serial $(STLINK_SERIAL)" \
	     -f target/$(OPENOCD_TARGET) \
	     -c "init; reset halt; \
	         load_image $< $(N657_SRAM_ADDR) bin; \
	         reg msplim_s 0x00000000; \
	         reg psplim_s 0x00000000; \
	         reg msp $$INIT_SP; \
	         mww 0xE000ED08 $(N657_SRAM_ADDR); \
	         mww 0xE000ED28 0xFFFFFFFF; \
	         resume $$ENTRY_T; \
	         shutdown"
else ifeq ($(BOARD),h7s3)
# H7S3L8 has only 64 KB user flash (FSBL/CCB use) -- no main flash. Load
# the .bin to AXI SRAM at 0x24000000 via OpenOCD load_image, then extract
# the initial SP/PC from the vector table and resume. Cortex-M7 (no MSPLIM
# register), so the M55-specific msplim_s writes from the N657 recipe are
# omitted.
H7S3_SRAM_ADDR := 0x24000000
flash: $(BUILD_DIR)/app.bin
	@INIT_SP=$$(od -A n -t x4 -N 4 $< | awk '{print "0x"$$1}'); \
	 ENTRY=$$(od -A n -j 4 -t x4 -N 4 $< | awk '{print "0x"$$1}'); \
	 ENTRY_T=$$(printf "0x%08x" $$(( $$ENTRY | 1 ))); \
	 echo "  H7S3 SRAM-load SP=$$INIT_SP entry=$$ENTRY_T"; \
	 openocd -f interface/stlink.cfg \
	     -c "adapter serial $(STLINK_SERIAL)" \
	     -f target/$(OPENOCD_TARGET) \
	     -c "init; reset halt; \
	         load_image $< $(H7S3_SRAM_ADDR) bin; \
	         reg msp $$INIT_SP; \
	         mww 0xE000ED08 $(H7S3_SRAM_ADDR); \
	         resume $$ENTRY_T; \
	         shutdown"
else
# Allow per-board overrides for OpenOCD binary and script search path
# (C5 needs the ST CubeIDE binary because stm32c5x.cfg isn't in upstream
# yet). Defaults are plain `openocd` from PATH and its built-in scripts.
OPENOCD              ?= openocd
OPENOCD_INTERFACE    ?= stlink.cfg
OPENOCD_SCRIPTS_FLAG := $(if $(OPENOCD_SCRIPTS),-s $(OPENOCD_SCRIPTS),)
CUBE_PROGRAMMER      ?= $(HOME)/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI

flash: $(BUILD_DIR)/app.elf
ifdef USE_CUBE_PROGRAMMER
	$(CUBE_PROGRAMMER) -c port=swd sn=$(STLINK_SERIAL) mode=UR \
	    -d $< -v -hardRst
else ifdef STLINK_SERIAL
	$(OPENOCD) $(OPENOCD_SCRIPTS_FLAG) \
	    -f interface/$(OPENOCD_INTERFACE) \
	    -c "adapter serial $(STLINK_SERIAL)" \
	    -f target/$(OPENOCD_TARGET) \
	    -c "program $< verify reset exit"
else
	$(OPENOCD) $(OPENOCD_SCRIPTS_FLAG) \
	    -f interface/$(OPENOCD_INTERFACE) -f target/$(OPENOCD_TARGET) \
	    -c "program $< verify reset exit"
endif
endif

size-report:
	@echo "== STM32_Bare_Test size report =="
	@for d in build/*; do \
	  if [ -f $$d/app.elf ]; then \
	    echo "--- $$d ---"; \
	    $(SIZE) $$d/app.elf; \
	  fi \
	done

clean:
	rm -rf build/$(BOARD)-$(BUILD)-$(TARGET)-$(CONFIG)

distclean:
	rm -rf build

-include $(DEPS)
