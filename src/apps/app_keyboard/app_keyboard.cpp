/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_keyboard.h"
#include "assets/keyboard_big.h"
#include "assets/keyboard_small.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <assets.h>
#include <esp_system.h>

using namespace mooncake;
using namespace smooth_ui_toolkit;

AppKeyboard::AppKeyboard()
{
    setAppInfo().name     = "Keyboard";
    setAppInfo().userData = new AppIcon_t(image_data_keyboard_big, image_data_keyboard_small);
}

AppKeyboard::~AppKeyboard()
{
    delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppKeyboard::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _info_update_time   = 0;
    _is_selecting       = true;
    _is_keyboard_active = false;
    _last_ble_connected = false;
    _last_usb_connected = false;
    _keyboard_type      = KeyboardSelectorMenu::KEYBOARD_TYPE_NONE;
    _show_restart_confirm = false;
    _restart_confirm_dirty = false;
    _restart_confirm_choice = 0;

    // Create and initialize selector menu
    _selector_menu = new KeyboardSelectorMenu();
    _selector_menu->init();
    select_keyboard_type();
}

void AppKeyboard::onRunning()
{
    if (_show_restart_confirm) {
        update_restart_confirm();
        return;
    }

    if (_is_selecting && _selector_menu) {
        // Update selector menu
        _selector_menu->update();

        // Check if selection is made
        if (_selector_menu->isSelected()) {
            auto keyboard_type = _selector_menu->getSelectedType();
            _is_selecting      = false;
            _keyboard_type     = keyboard_type;

            // Destroy selector menu
            delete _selector_menu;
            _selector_menu = nullptr;

            // Initialize selected keyboard type
            if (keyboard_type == KeyboardSelectorMenu::KEYBOARD_TYPE_BLE) {
                mclog::tagInfo(getAppInfo().name, "keyboard type selected: BLE");
                init_ble_keyboard();
            } else if (keyboard_type == KeyboardSelectorMenu::KEYBOARD_TYPE_USB) {
                mclog::tagInfo(getAppInfo().name, "keyboard type selected: USB");
                init_usb_keyboard();
            }

            render_keyboard_interface();
            _is_keyboard_active = true;
        }
    } else if (_is_keyboard_active) {
        // Update connection info periodically
        update_connection_info();
    }

    // Close app when home button clicked
    if (GetHAL().homeButton.wasClicked()) {
        if (_is_keyboard_active && _keyboard_type == KeyboardSelectorMenu::KEYBOARD_TYPE_USB) {
            _show_restart_confirm = true;
            _restart_confirm_choice = 0;
            _restart_confirm_dirty = true;
        } else {
            close();
        }
    }
}

void AppKeyboard::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    // Clean up selector menu if still exists
    if (_selector_menu) {
        delete _selector_menu;
        _selector_menu = nullptr;
    }

    if (_is_keyboard_active) {
        GetHAL().usbKeyboardDeinit();
        close();
        return;
    }

    close();
}

void AppKeyboard::select_keyboard_type()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.println("[Select Keyboard Type]");
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.println("Use arrow keys to select");
    GetHAL().canvas.println("Press Enter to confirm");
    GetHAL().pushCanvas();
}

void AppKeyboard::init_ble_keyboard()
{
    mclog::tagInfo(getAppInfo().name, "initializing BLE keyboard");
    GetHAL().bleKeyboardInit();
    mclog::tagInfo(getAppInfo().name, "BLE enabled, device name: {}", GetHAL().getBleKeyboardName());
}

void AppKeyboard::init_usb_keyboard()
{
    mclog::tagInfo(getAppInfo().name, "initializing USB keyboard");
    GetHAL().usbKeyboardInit();
    mclog::tagInfo(getAppInfo().name, "USB HID enabled");
}

void AppKeyboard::update_connection_info()
{
    // Update connection status every 2 seconds
    if (GetHAL().millis() - _info_update_time > 1000) {
        bool ble_connected = GetHAL().bleKeyboardIsConnected();
        bool usb_connected = GetHAL().usbKeyboardIsConnected();

        if (ble_connected != _last_ble_connected) {
            mclog::tagInfo(getAppInfo().name, "BLE connection: {}", ble_connected ? "connected" : "disconnected");
            _last_ble_connected = ble_connected;
        }
        if (usb_connected != _last_usb_connected) {
            mclog::tagInfo(getAppInfo().name, "USB connection: {}", usb_connected ? "connected" : "disconnected");
            _last_usb_connected = usb_connected;
        }

        render_connection_status();
        _info_update_time = GetHAL().millis();
    }
}

void AppKeyboard::render_keyboard_interface()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.println("Keyboard Mode Active");

    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.println("Type to send keys...");
    GetHAL().pushCanvas();
}

void AppKeyboard::render_connection_status()
{
    // Clear status area
    GetHAL().canvas.fillRect(0, 32, GetHAL().canvas.width(), 20, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 32);

    // Check both BLE and USB connections
    bool ble_connected = GetHAL().bleKeyboardIsConnected();
    bool usb_connected = GetHAL().usbKeyboardIsConnected();

    if (ble_connected) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.printf("BLE: Connected");
    } else if (usb_connected) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.printf("USB: Connected");
    } else {
        GetHAL().canvas.setTextColor(TFT_RED, THEME_COLOR_BG);
        GetHAL().canvas.printf("Disconnected");
    }

    GetHAL().pushCanvas();
}

void AppKeyboard::render_restart_confirm()
{
    auto& canvas = GetHAL().canvas;
    canvas.fillScreen(THEME_COLOR_BG);
    canvas.setTextSize(1);
    canvas.setCursor(0, 0);
    canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
    canvas.println("Restart required");
    canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    canvas.println("USB Serial/JTAG won't");
    canvas.println("return without restart.");
    canvas.println("");
    canvas.println("Restart now?");

    int button_w = canvas.width() / 2;
    int button_h = 24;
    int y        = canvas.height() - button_h - 4;
    int cancel_x = 0;
    int ok_x     = canvas.width() - button_w;

    uint16_t cancel_bg = (_restart_confirm_choice == 0) ? TFT_RED : TFT_DARKGREY;
    uint16_t ok_bg     = (_restart_confirm_choice == 1) ? TFT_GREEN : TFT_DARKGREY;

    canvas.fillRect(cancel_x, y, button_w, button_h, cancel_bg);
    canvas.fillRect(ok_x, y, button_w, button_h, ok_bg);

    canvas.setTextColor(TFT_WHITE, cancel_bg);
    canvas.setCursor(cancel_x + 12, y + 8);
    canvas.print("Cancel");
    uint16_t ok_text = (_restart_confirm_choice == 1) ? TFT_BLACK : TFT_WHITE;
    canvas.setTextColor(ok_text, ok_bg);
    canvas.setCursor(ok_x + 22, y + 8);
    canvas.print("OK");

    canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    canvas.setCursor(0, y - 12);
    canvas.print("Use arrows + Enter");

    GetHAL().pushCanvas();
}

void AppKeyboard::update_restart_confirm()
{
    if (_restart_confirm_dirty) {
        render_restart_confirm();
        _restart_confirm_dirty = false;
    }

    auto event = GetHAL().keyboard.getLatestKeyEventRaw();
    if (!event.state) {
        return;
    }

    bool changed = false;
    if ((event.row == 3 && event.col == 10) || (event.row == 2 && event.col == 11)) {
        if (_restart_confirm_choice != 0) {
            _restart_confirm_choice = 0;
            changed = true;
        }
    } else if ((event.row == 3 && event.col == 12) || (event.row == 3 && event.col == 11)) {
        if (_restart_confirm_choice != 1) {
            _restart_confirm_choice = 1;
            changed = true;
        }
    } else if (event.row == 2 && event.col == 13) {
        if (_restart_confirm_choice == 1) {
            GetHAL().usbKeyboardDeinit();
            GetHAL().usbSwitchToSerialJtag();
            delay(100);
            esp_restart();
        } else {
            _show_restart_confirm = false;
            render_keyboard_interface();
            render_connection_status();
        }
        return;
    }

    if (changed) {
        render_restart_confirm();
    }
}
