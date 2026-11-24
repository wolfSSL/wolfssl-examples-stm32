# NUCLEO-C5A3ZG secure OTA demo: wolfBoot + wolfIP + wolfSSL (TLS 1.3, ML-KEM)
#
#   make                      build build/app.bin (unsigned, link address behind
#                             the wolfBoot header in the BOOT partition)
#   make BENCH=1              also run the TLS handshake benchmark at boot
#   make OTA_GROUP=WOLFSSL_SECP256R1MLKEM768   key share used for the OTA check
#
# wolfBoot must be configured and built first (it generates include/target.h);
# demo/prepare.sh does both and signs the images.

CROSS      ?= arm-none-eabi-
CC         := $(CROSS)gcc
OBJCOPY    := $(CROSS)objcopy
SIZE       := $(CROSS)size

TOP           := $(abspath .)
WOLFSSL_ROOT  ?= $(abspath $(TOP)/../../wolfssl)
WOLFIP_ROOT   ?= $(abspath $(TOP)/../../wolfip)
WOLFBOOT_ROOT ?= $(abspath $(TOP)/../../wolfboot)
CUBEC5_INC    ?= $(HOME)/GitHub/STM32CubeC5/stm32c5xx_dfp/Include
STM32CUBE_FW_U5 ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0
CMSIS_CORE    ?= $(STM32CUBE_FW_U5)/Drivers/CMSIS/Core/Include
BUILD         ?= $(TOP)/build

# Must match the wolfBoot .config used to sign the image
IMAGE_HEADER_SIZE ?= 8192
BOOT_ADDRESS      ?= 0x08010000
PARTITION_SIZE    ?= 0x70000
SECTOR_SIZE       ?= 0x2000
APP_ORIGIN := $(shell printf '0x%08X' $$(( $(BOOT_ADDRESS) + $(IMAGE_HEADER_SIZE) )))
APP_SIZE   := $(shell printf '0x%X' $$(( $(PARTITION_SIZE) - 2 * $(SECTOR_SIZE) - $(IMAGE_HEADER_SIZE) )))

CPU := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
CFLAGS := $(CPU) -Os -g -ffunction-sections -fdata-sections -Wall -Wextra \
    -Wno-unused-parameter -DSTM32C5 -DSTM32C5A3xx -DWOLFSSL_USER_SETTINGS \
    -DWOLFSSL_WOLFIP -I$(TOP)/include -I$(WOLFIP_ROOT) -I$(WOLFIP_ROOT)/src \
    -I$(WOLFIP_ROOT)/src/port -I$(WOLFIP_ROOT)/src/port/stm32 -I$(WOLFSSL_ROOT) \
    -I$(CUBEC5_INC) -I$(CMSIS_CORE)
DEMO_BUILD_EPOCH ?= $(shell date +%s)
CFLAGS += -DDEMO_BUILD_EPOCH=$(DEMO_BUILD_EPOCH)
ifeq ($(BENCH),1)
  CFLAGS += -DOTA_BENCH_AT_BOOT
endif
ifneq ($(OTA_GROUP),)
  CFLAGS += -DOTA_GROUP=$(OTA_GROUP)
endif
ifneq ($(OTA_SERVER_IP),)
  CFLAGS += -DOTA_SERVER_IP=\"$(OTA_SERVER_IP)\"
endif

# libwolfboot + the wolfBoot STM32C5 flash HAL, built the way wolfBoot builds
# its own test-app (RAM_CODE flash routines, write-once trailer handling).
WB_CFLAGS := $(CPU) -Os -g -ffunction-sections -fdata-sections -mlong-calls \
    -DTARGET_stm32c5 -DCORTEX_M33 -DARCH_ARM -DRAM_CODE -DNVM_FLASH_WRITEONCE \
    -DARCH_FLASH_OFFSET=0x08000000 -DWOLFBOOT_ORIGIN=0x08000000 \
    -DWOLFBOOT_HASH_SHA256 -DIMAGE_HEADER_SIZE=$(IMAGE_HEADER_SIZE) \
    -DWOLFBOOT_SIGN_ML_DSA -I$(WOLFBOOT_ROOT) -I$(WOLFBOOT_ROOT)/include \
    -I$(TOP)/include

APP_SRCS := $(TOP)/src/main.c \
    $(WOLFIP_ROOT)/src/port/stm32c5a3/startup.c \
    $(WOLFIP_ROOT)/src/port/stm32c5a3/syscalls.c \
    $(WOLFIP_ROOT)/src/wolfip.c \
    $(WOLFIP_ROOT)/src/port/stm32/stm32_eth.c \
    $(WOLFIP_ROOT)/src/port/wolfssl_io.c

WB_SRCS := $(TOP)/src/ota_flash.c \
    $(WOLFBOOT_ROOT)/src/libwolfboot.c \
    $(WOLFBOOT_ROOT)/hal/stm32c5.c

WC := $(WOLFSSL_ROOT)/wolfcrypt/src
SSL_SRCS := $(addprefix $(WOLFSSL_ROOT)/src/, ssl.c tls.c tls13.c internal.c \
        keys.c wolfio.c) \
    $(addprefix $(WC)/, aes.c sha.c sha256.c sha512.c sha3.c hmac.c hash.c \
        kdf.c random.c ecc.c asn.c coding.c wc_port.c memory.c wolfmath.c \
        sp_int.c sp_cortexm.c curve25519.c fe_operations.c wc_mlkem.c \
        wc_mlkem_poly.c chacha.c chacha20_poly1305.c poly1305.c signature.c \
        logging.c error.c wc_encrypt.c) \
    $(WC)/port/st/stm32.c \
    $(addprefix $(WC)/port/arm/, thumb2-sha256-asm_c.c thumb2-sha512-asm_c.c \
        thumb2-sha3-asm_c.c thumb2-chacha-asm_c.c thumb2-poly1305-asm_c.c \
        thumb2-curve25519_c.c thumb2-mlkem-asm_c.c thumb2-aes-asm_c.c)

obj = $(patsubst /%.c,$(BUILD)/obj/%.o,$(1))
APP_OBJS := $(call obj,$(APP_SRCS))
WB_OBJS  := $(call obj,$(WB_SRCS))
SSL_OBJS := $(call obj,$(SSL_SRCS))
DEPS := $(APP_OBJS:.o=.d) $(WB_OBJS:.o=.d) $(SSL_OBJS:.o=.d)

LDFLAGS := $(CPU) -T$(BUILD)/target.ld -Wl,--gc-sections \
    -Wl,-Map=$(BUILD)/app.map -specs=nano.specs -specs=nosys.specs

all: $(BUILD)/app.bin

$(BUILD)/target.ld: target.ld.in Makefile
	@mkdir -p $(dir $@)
	sed -e 's/@APP_ORIGIN@/$(APP_ORIGIN)/' -e 's/@APP_SIZE@/$(APP_SIZE)/' $< > $@

$(APP_OBJS): $(BUILD)/obj/%.o: /%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(SSL_OBJS): $(BUILD)/obj/%.o: /%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(WB_OBJS): $(BUILD)/obj/%.o: /%.c
	@mkdir -p $(dir $@)
	$(CC) $(WB_CFLAGS) -MMD -c $< -o $@

$(BUILD)/app.elf: $(APP_OBJS) $(WB_OBJS) $(SSL_OBJS) $(BUILD)/target.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(WB_OBJS) $(SSL_OBJS) -o $@
	$(SIZE) $@

$(BUILD)/app.bin: $(BUILD)/app.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD)

-include $(DEPS)

.PHONY: all clean
