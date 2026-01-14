/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB MSC device
 * @return true on success, false on failure
 */
bool usb_msc_device_init(void);

/**
 * @brief Deinitialize USB MSC device
 */
void usb_msc_device_deinit(void);

/**
 * @brief Start USB MSC device
 * @return true on success, false on failure
 */
bool usb_msc_device_start(void);

/**
 * @brief Stop USB MSC device
 * @return true on success, false on failure
 */
bool usb_msc_device_stop(void);

/**
 * @brief Check if USB MSC device is active
 * @return true if active, false otherwise
 */
bool usb_msc_device_is_active(void);

#ifdef __cplusplus
}
#endif
