/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <cstdbool>

/**
 * Initialize USB MSC (Mass Storage Class) to expose SD card to host computer
 * Uses custom esp-idf TinyUSB component
 */
void usb_msc_helper_init(void);

/**
 * Disable USB MSC and unmount SD card from host
 */
void usb_msc_helper_deinit(void);

/**
 * Check if USB MSC is currently active
 */
bool usb_msc_helper_is_active(void);

/**
 * Toggle USB MSC on/off
 * @return true if toggle successful, false otherwise
 */
bool usb_msc_helper_toggle(void);
