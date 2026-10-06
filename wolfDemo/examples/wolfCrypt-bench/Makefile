# ---------------------------------------------------------------------------
# wolfDemo (STM32U585CI) - wolfCrypt benchmark / test
#
# Board: wolfDemo with STM32U585CIT6
# UART:  USART1 on PA9/PA10 via the CH340G (USB-C), 115200 8N1
# Clock: HSE 8 MHz -> PLL1 -> 160 MHz
#
# Usage:
#   make                        # wolfCrypt benchmark, STM32 HW crypto
#   make CONFIG=c    bench      # wolfCrypt benchmark, pure software
#   make CONFIG=hw   test       # wolfCrypt test, STM32 HW crypto
#   make CONFIG=c    test       # wolfCrypt test, pure software
#   make size-report            # aggregate size across all built configs
#   make flash                  # OpenOCD flash (same TARGET / CONFIG vars)
#
# Make/env variables:
#   WOLFSSL_ROOT      = ../../../../wolfssl         (wolfSSL source tree)
#   STM32CUBE_FW_U5   = $(HOME)/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0
#   OPENOCD_INTERFACE = stlink                      (or jlink)
# ---------------------------------------------------------------------------

# ----- Toolchain -----------------------------------------------------------
CROSS        ?= arm-none-eabi-
CC            = $(CROSS)gcc
AS            = $(CROSS)gcc -x assembler-with-cpp
OBJCOPY       = $(CROSS)objcopy
SIZE          = $(CROSS)size

# ----- Repos / paths -------------------------------------------------------
TOP          := $(abspath .)
# Repo layout: .../<dir>/wolfssl-examples-stm32/wolfDemo/examples/wolfCrypt-bench/
# Siblings (e.g. wolfssl) live at .../<dir>/<repo> - 4 levels up from $(TOP).
REPO_SIBLING := $(abspath $(TOP)/../../../..)

WOLFSSL_ROOT    ?= $(REPO_SIBLING)/wolfssl
STM32CUBE_FW_U5 ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0

# ----- Target / build config -----------------------------------------------
TARGET   ?= bench
CONFIG   ?= hw

ifeq ($(filter $(TARGET),test bench),)
  $(error TARGET must be one of: test | bench)
endif
ifeq ($(filter $(CONFIG),c hw),)
  $(error CONFIG must be one of: c | hw)
endif

BUILD_DIR := $(TOP)/build/$(TARGET)-$(CONFIG)

# ----- MCU / arch flags ----------------------------------------------------
MCU_FLAGS := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
CPU_DEFS  := -DSTM32U585xx -DUSE_HAL_DRIVER

# ----- Variant overlays ----------------------------------------------------
VARIANT_DEFS := -DBUILD_CONFIG_NAME=\"$(CONFIG)\"
ifeq ($(CONFIG),hw)
  VARIANT_DEFS += -DWOLFDEMO_CONFIG_HW
endif
ifeq ($(TARGET),bench)
  VARIANT_DEFS += -DWOLFDEMO_BENCH
endif

# ----- Include paths -------------------------------------------------------
INCLUDES := \
  -I$(TOP) \
  -I$(TOP)/src \
  -I$(STM32CUBE_FW_U5)/Drivers/CMSIS/Core/Include \
  -I$(STM32CUBE_FW_U5)/Drivers/CMSIS/Device/ST/STM32U5xx/Include \
  -I$(STM32CUBE_FW_U5)/Drivers/STM32U5xx_HAL_Driver/Inc \
  -I$(STM32CUBE_FW_U5)/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy \
  -I$(WOLFSSL_ROOT)

# ----- Compile / link flags ------------------------------------------------
COMMON_DEFS := -DWOLFSSL_USER_SETTINGS $(CPU_DEFS) $(VARIANT_DEFS)

# -O2 + -fomit-frame-pointer required for the SP math inline ASM register
# allocation
CFLAGS    = $(MCU_FLAGS) $(INCLUDES) $(COMMON_DEFS) \
            -O2 -g -fomit-frame-pointer \
            -ffunction-sections -fdata-sections \
            -Wall -Wno-unused-function -Wno-unused-variable \
            -fno-common -fno-strict-aliasing \
            -MMD -MP -std=gnu11

ASFLAGS  := $(MCU_FLAGS) -c -g3

LDSCRIPT := $(TOP)/linker/stm32u585ci_flat.ld
# newlib-nano needs _printf_float for the benchmark's %f output
LDFLAGS  := $(MCU_FLAGS) \
            -T$(LDSCRIPT) \
            --specs=nosys.specs --specs=nano.specs \
            -u _printf_float \
            -Wl,--gc-sections \
            -Wl,-Map=$(BUILD_DIR)/app.map \
            -Wl,--print-memory-usage

LDLIBS   := -lc -lm

# ----- Sources -------------------------------------------------------------
# Application / glue
SRC_C  := \
  src/main_$(TARGET).c \
  src/hw_init.c \
  src/leds.c \
  src/stubs.c \
  src/system_stm32u5xx.c \
  src/stm32u5xx_it.c

SRC_ASM := src/startup_stm32u585xx.s

# STM32Cube HAL - compile only modules enabled in hal_conf.h
HAL_SRC_DIR := $(STM32CUBE_FW_U5)/Drivers/STM32U5xx_HAL_Driver/Src
HAL_SOURCES := \
  stm32u5xx_hal.c \
  stm32u5xx_hal_cortex.c \
  stm32u5xx_hal_dma.c \
  stm32u5xx_hal_dma_ex.c \
  stm32u5xx_hal_exti.c \
  stm32u5xx_hal_flash.c \
  stm32u5xx_hal_flash_ex.c \
  stm32u5xx_hal_gpio.c \
  stm32u5xx_hal_icache.c \
  stm32u5xx_hal_pwr.c \
  stm32u5xx_hal_pwr_ex.c \
  stm32u5xx_hal_rcc.c \
  stm32u5xx_hal_rcc_ex.c \
  stm32u5xx_hal_rng.c \
  stm32u5xx_hal_rng_ex.c \
  stm32u5xx_hal_tim.c \
  stm32u5xx_hal_tim_ex.c \
  stm32u5xx_hal_uart.c \
  stm32u5xx_hal_uart_ex.c

# HW crypto variant adds AES + HASH + PKA drivers
ifeq ($(CONFIG),hw)
HAL_SOURCES += \
  stm32u5xx_hal_cryp.c \
  stm32u5xx_hal_cryp_ex.c \
  stm32u5xx_hal_hash.c \
  stm32u5xx_hal_hash_ex.c \
  stm32u5xx_hal_pka.c
endif

HAL_SRC := $(addprefix $(HAL_SRC_DIR)/,$(HAL_SOURCES))

# wolfCrypt - compile the whole wolfcrypt/src tree; user_settings gates out
# what isn't needed.
WC_SRC_DIR := $(WOLFSSL_ROOT)/wolfcrypt/src
WC_ALL     := $(wildcard $(WC_SRC_DIR)/*.c)
# Always exclude these
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

# Target entry point
ifeq ($(TARGET),test)
  SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/test/test.c
endif
ifeq ($(TARGET),bench)
  SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/benchmark/benchmark.c
endif

ALL_C_SRC := $(SRC_C) $(WC_SRC) $(HAL_SRC)

# ----- Object files --------------------------------------------------------
rel_obj = $(BUILD_DIR)/obj/$(subst /,__,$(patsubst /%,%,$(1))).o

C_OBJ   := $(foreach s,$(ALL_C_SRC),$(call rel_obj,$(s)))
ASM_OBJ := $(foreach s,$(SRC_ASM),$(call rel_obj,$(s)))
OBJS    := $(C_OBJ) $(ASM_OBJ)
DEPS    := $(OBJS:.o=.d)

# ----- Default target -------------------------------------------------------
.PHONY: all test bench build check-deps flash size clean distclean size-report

all: build

test:
	$(MAKE) build TARGET=test

bench:
	$(MAKE) build TARGET=bench

build: check-deps $(BUILD_DIR)/app.bin $(BUILD_DIR)/app.hex size

check-deps:
	@test -f $(WOLFSSL_ROOT)/wolfssl/wolfcrypt/settings.h || \
	  { echo "wolfSSL not found at WOLFSSL_ROOT=$(WOLFSSL_ROOT)"; exit 1; }
	@test -d $(HAL_SRC_DIR) || \
	  { echo "STM32CubeU5 not found at STM32CUBE_FW_U5=$(STM32CUBE_FW_U5)"; exit 1; }

# ----- Rules ----------------------------------------------------------------
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/obj

define COMPILE_C_TEMPLATE
$(call rel_obj,$(1)): $(1) | $(BUILD_DIR)
	@echo "  CC   $$<"
	@mkdir -p $$(@D)
	@$(CC) $(CFLAGS) -c $$< -o $$@
endef

define COMPILE_ASM_TEMPLATE
$(call rel_obj,$(1)): $(1) | $(BUILD_DIR)
	@echo "  AS   $$<"
	@mkdir -p $$(@D)
	@$(AS) $(ASFLAGS) -c $$< -o $$@
endef

$(foreach s,$(ALL_C_SRC),$(eval $(call COMPILE_C_TEMPLATE,$(s))))
$(foreach s,$(SRC_ASM),$(eval $(call COMPILE_ASM_TEMPLATE,$(s))))

$(BUILD_DIR)/app.elf: $(OBJS) $(LDSCRIPT)
	@echo "  LD   $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/app.bin: $(BUILD_DIR)/app.elf
	@echo "  BIN  $@"
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/app.hex: $(BUILD_DIR)/app.elf
	@$(OBJCOPY) -O ihex $< $@

size: $(BUILD_DIR)/app.elf
	@echo
	@echo "----- size: $(TARGET)-$(CONFIG) -----"
	@$(SIZE) $<

OPENOCD_INTERFACE ?= stlink

flash: check-deps $(BUILD_DIR)/app.elf
	openocd -f interface/$(OPENOCD_INTERFACE).cfg \
	    $(if $(filter jlink,$(OPENOCD_INTERFACE)),-c "transport select swd") \
	    -f target/stm32u5x.cfg \
	    -c "program $(BUILD_DIR)/app.elf verify reset exit"

size-report:
	@echo "== wolfDemo wolfCrypt-bench size report =="
	@for d in build/*; do \
	  if [ -f $$d/app.elf ]; then \
	    echo "--- $$d ---"; \
	    $(SIZE) $$d/app.elf; \
	  fi \
	done

clean:
	rm -rf build/$(TARGET)-$(CONFIG)

distclean:
	rm -rf build

# ----- Dependency files -----------------------------------------------------
-include $(DEPS)
