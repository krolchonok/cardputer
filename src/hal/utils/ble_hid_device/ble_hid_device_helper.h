/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// https://github.com/espressif/esp-idf/blob/v5.4.2/examples/bluetooth/esp_hid_device
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_HID_DEVICE_STATE_IDLE = 0,
    BLE_HID_DEVICE_STATE_CONNECTED,
} BleHidDeviceState_t;

bool ble_hid_device_helper_init(void);
void ble_hid_device_helper_send(uint8_t* buffer);
BleHidDeviceState_t ble_hid_device_helper_get_state(void);
const char* ble_hid_device_helper_get_device_name(void);
void ble_hid_device_helper_start_advertising(void);

#ifdef __cplusplus
}
#endif
