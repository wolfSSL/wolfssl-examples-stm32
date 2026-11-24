/* ota_flash.c
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

/* Thin wrapper over libwolfboot + the wolfBoot STM32C5 flash HAL, kept in its
 * own translation unit so the wolfBoot register headers never meet CMSIS. */

#include <stdint.h>
#include <string.h>
#include "target.h"
#include "hal.h"
#include "wolfboot/wolfboot.h"
#include "ota_flash.h"

/* C5 flash programs 128-bit quad-words with per-quad-word ECC; every write
 * must be 16-byte aligned and a multiple of 16 bytes. */
#define OTA_WRITE_QUANTUM 16u

uint32_t ota_fw_version(void)
{
    return wolfBoot_current_firmware_version();
}

uint32_t ota_update_version(void)
{
    return wolfBoot_update_firmware_version();
}

int ota_boot_confirm(void)
{
    uint8_t st = 0;

    if (wolfBoot_get_partition_state(PART_BOOT, &st) == 0 &&
            st == IMG_STATE_TESTING) {
        wolfBoot_success();
        return 1;
    }
    return 0;
}

/* A triggered update that wolfBoot did not install (failed verification or
 * a refused downgrade) leaves the UPDATE partition in the updating state. */
int ota_update_refused(uint32_t *ver)
{
    uint8_t st = 0;

    if (wolfBoot_get_partition_state(PART_UPDATE, &st) == 0 &&
            st == IMG_STATE_UPDATING) {
        *ver = wolfBoot_update_firmware_version();
        return 1;
    }
    return 0;
}

uint32_t ota_update_max(void)
{
    /* wolfBoot keeps the last sector of the partition for its trailer. */
    return WOLFBOOT_PARTITION_SIZE - WOLFBOOT_SECTOR_SIZE;
}

int ota_update_erase(uint32_t len)
{
    int ret;
    uint32_t sz;

    if (len > ota_update_max())
        return -1;
    sz = (len + WOLFBOOT_SECTOR_SIZE - 1u) & ~(WOLFBOOT_SECTOR_SIZE - 1u);
    hal_flash_unlock();
    ret = hal_flash_erase(WOLFBOOT_PARTITION_UPDATE_ADDRESS, (int)sz);
    /* also the trailer sector, so a state left by a refused update is gone */
    if (ret == 0)
        ret = hal_flash_erase(WOLFBOOT_PARTITION_UPDATE_ADDRESS +
            WOLFBOOT_PARTITION_SIZE - WOLFBOOT_SECTOR_SIZE,
            WOLFBOOT_SECTOR_SIZE);
    hal_flash_lock();
    return ret;
}

int ota_update_write(uint32_t off, const uint8_t *buf, uint32_t len)
{
    int ret;

    if ((off % OTA_WRITE_QUANTUM) != 0u || (len % OTA_WRITE_QUANTUM) != 0u ||
            off + len > ota_update_max())
        return -1;
    hal_flash_unlock();
    ret = hal_flash_write(WOLFBOOT_PARTITION_UPDATE_ADDRESS + off, buf,
        (int)len);
    hal_flash_lock();
    return ret;
}

void ota_update_trigger(void)
{
    wolfBoot_update_trigger();
}
