/* ota_flash.h
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

#ifndef OTA_FLASH_H
#define OTA_FLASH_H

#include <stdint.h>

uint32_t ota_fw_version(void);
uint32_t ota_update_version(void);
int ota_boot_confirm(void);
int ota_update_refused(uint32_t *ver);
uint32_t ota_update_max(void);
int ota_update_erase(uint32_t len);
int ota_update_write(uint32_t off, const uint8_t *buf, uint32_t len);
void ota_update_trigger(void);

#endif /* OTA_FLASH_H */
