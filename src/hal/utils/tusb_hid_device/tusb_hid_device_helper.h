/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// https://github.com/espressif/esp-idf/blob/v5.4.2/examples/peripherals/usb/device/tusb_hid
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void tusb_hid_device_helper_init(void);
void tusb_hid_device_helper_deinit(void);
void tusb_hid_device_helper_report(uint8_t modifier, uint8_t* keycode);
bool tusb_hid_device_helper_is_mounted(void);
void tusb_hid_device_helper_switch_to_serial_jtag(void);
void tusb_hid_device_helper_release_keys(void);

#ifdef __cplusplus
}
#endif
