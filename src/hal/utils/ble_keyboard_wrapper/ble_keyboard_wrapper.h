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

#ifdef __cplusplus
}
#endif
