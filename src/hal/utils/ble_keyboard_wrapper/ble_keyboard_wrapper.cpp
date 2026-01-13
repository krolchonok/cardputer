/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "ble_keyboard_wrapper.h"
#include <BleKeyboard.h>

static BleKeyboard* _bleKeyboard = nullptr;

bool ble_keyboard_wrapper_init(const char* deviceName)
{
    if (_bleKeyboard != nullptr) {
        return true;  // Already initialized
    }

    _bleKeyboard = new BleKeyboard(deviceName, "M5Stack", 100);
    _bleKeyboard->begin();
    return true;
}

bool ble_keyboard_wrapper_is_connected(void)
{
    if (_bleKeyboard == nullptr) {
        return false;
    }
    return _bleKeyboard->isConnected();
}

void ble_keyboard_wrapper_press(uint8_t keyCode)
{
    if (_bleKeyboard != nullptr && _bleKeyboard->isConnected()) {
        _bleKeyboard->press(keyCode);
    }
}

void ble_keyboard_wrapper_release(uint8_t keyCode)
{
    if (_bleKeyboard != nullptr && _bleKeyboard->isConnected()) {
        _bleKeyboard->release(keyCode);
    }
}

void ble_keyboard_wrapper_release_all(void)
{
    if (_bleKeyboard != nullptr && _bleKeyboard->isConnected()) {
        _bleKeyboard->releaseAll();
    }
}
