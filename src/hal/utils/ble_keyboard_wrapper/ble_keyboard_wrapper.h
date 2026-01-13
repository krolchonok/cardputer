/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ble_keyboard_wrapper_init(const char* deviceName);
bool ble_keyboard_wrapper_is_connected(void);
void ble_keyboard_wrapper_press(uint8_t keyCode);
void ble_keyboard_wrapper_release(uint8_t keyCode);
void ble_keyboard_wrapper_release_all(void);

// Send raw HID report: modifiers + up to 6 keycodes (keys array may be NULL to indicate none)
void ble_keyboard_wrapper_send_report(uint8_t modifiers, const uint8_t* keys /*size 6*/);

#ifdef __cplusplus
}
#endif
