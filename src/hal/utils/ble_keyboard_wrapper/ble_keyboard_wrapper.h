/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 * 
 * Unified BLE HID implementation for Keyboard + Media Control
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BLE HID Device States
 */
typedef enum {
    BLE_HID_STATE_IDLE = 0,
    BLE_HID_STATE_ADVERTISING,
    BLE_HID_STATE_CONNECTED,
} BleHidState_t;

/**
 * @brief Initialize BLE HID Keyboard with the given device name.
 * @param deviceName Device name to advertise
 * @return true on success, false on failure
 */
bool ble_keyboard_wrapper_init(const char* deviceName);

/**
 * @brief Deinitialize BLE HID Keyboard and free resources.
 */
void ble_keyboard_wrapper_deinit(void);

/**
 * @brief Check if a BLE host is connected.
 * @return true if connected, false otherwise
 */
bool ble_keyboard_wrapper_is_connected(void);

/**
 * @brief Get current BLE HID state.
 * @return Current state
 */
BleHidState_t ble_keyboard_wrapper_get_state(void);

/**
 * @brief Start BLE advertising if not already advertising.
 */
void ble_keyboard_wrapper_start_advertising(void);

/**
 * @brief Stop BLE advertising.
 */
void ble_keyboard_wrapper_stop_advertising(void);

/**
 * @brief Press a key (will be held until released).
 * @param keyCode USB HID keycode
 */
void ble_keyboard_wrapper_press(uint8_t keyCode);

/**
 * @brief Release a specific key.
 * @param keyCode USB HID keycode
 */
void ble_keyboard_wrapper_release(uint8_t keyCode);

/**
 * @brief Release all pressed keys.
 */
void ble_keyboard_wrapper_release_all(void);

/**
 * @brief Send raw HID keyboard report.
 * @param modifiers Modifier byte (Ctrl, Shift, Alt, GUI)
 * @param keys Array of up to 6 keycodes (may be NULL for no keys)
 */
void ble_keyboard_wrapper_send_report(uint8_t modifiers, const uint8_t* keys);

/**
 * @brief Send media/consumer control key.
 * @param usageId HID Consumer Control usage ID (e.g., 0xE9 for Volume Up)
 * @param pressed true for key press, false for key release
 */
void ble_keyboard_wrapper_send_media_key(uint16_t usageId, bool pressed);

/**
 * @brief Clear all BLE bonding data (useful for fixing pairing issues).
 */
void ble_keyboard_wrapper_clear_bonding(void);

/**
 * @brief Get the device name.
 * @return Device name string
 */
const char* ble_keyboard_wrapper_get_device_name(void);

/**
 * @brief Set battery level (0-100).
 * @param level Battery level percentage
 */
void ble_keyboard_wrapper_set_battery_level(uint8_t level);

#ifdef __cplusplus
}
#endif
