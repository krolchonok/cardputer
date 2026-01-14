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
#include <hal/utils/ble_mouse_wrapper/ble_mouse_wrapper.h>

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
                render_keyboard_interface();
                _is_keyboard_active = true;
            } else if (keyboard_type == KeyboardSelectorMenu::KEYBOARD_TYPE_USB) {
                mclog::tagInfo(getAppInfo().name, "keyboard type selected: USB");
                init_usb_keyboard();
                render_keyboard_interface();
                _is_keyboard_active = true;
            } else if (keyboard_type == KeyboardSelectorMenu::KEYBOARD_TYPE_TIKTOK) {
                mclog::tagInfo(getAppInfo().name, "keyboard type selected: TikTok Controller");
                init_tiktok_controller();
                _is_tiktok_mode = true;
            }
        }
    } else if (_is_keyboard_active) {
        // Update connection info periodically
        update_connection_info();
    } else if (_is_tiktok_mode) {
        // Update TikTok controller
        update_tiktok_controller();
    }

    // Close app when home button clicked
    if (GetHAL().homeButton.wasClicked()) {
        if (_is_keyboard_active && _keyboard_type == KeyboardSelectorMenu::KEYBOARD_TYPE_USB) {
            _show_restart_confirm = true;
            _restart_confirm_choice = 0;
            _restart_confirm_dirty = true;
        } else if (_is_tiktok_mode) {
            // Just close the app for TikTok mode
            close();
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

    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.println(("BLE MAC: " + GetHAL().getBleMacString()).c_str());

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

void AppKeyboard::init_tiktok_controller()
{
    mclog::tagInfo(getAppInfo().name, "initializing TikTok controller");
    GetHAL().bleMouseInitWithName("TikTok Remote");
    _tiktok_mouse_positioned = false;
    _last_mouse_connected = false;
    render_tiktok_interface();
}

void AppKeyboard::render_tiktok_interface()
{
    auto& canvas = GetHAL().canvas;
    canvas.fillScreen(THEME_COLOR_BG);
    canvas.setTextColor(TFT_MAGENTA, THEME_COLOR_BG);
    canvas.setCursor(0, 0);
    canvas.setTextSize(1);
    canvas.println("[TikTok Controller]");

    canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    canvas.println("");
    canvas.println("UP: Swipe Up (Next)");
    canvas.println("DOWN: Swipe Down (Prev)");
    canvas.println("LEFT/RIGHT: Swipe L/R");
    canvas.println("ENTER: Like");
    canvas.println("SPACE: Pause");
    canvas.println("HOME: Center");
    canvas.println("");

    bool mouse_connected = GetHAL().bleMouseIsConnected();
    if (mouse_connected) {
        canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        canvas.println("BLE Mouse: Connected");
        if (_tiktok_mouse_positioned) {
            canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
            canvas.println("Cursor: Ready");
        }
    } else {
        canvas.setTextColor(TFT_RED, THEME_COLOR_BG);
        canvas.println("BLE Mouse: Waiting...");
        canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
        canvas.printf("Pair: %s\\n", GetHAL().getBleMouseName().c_str());
    }

    canvas.setTextColor(TFT_DARKGREY, THEME_COLOR_BG);
    canvas.printf("MAC: %s", GetHAL().getBleMacString().c_str());

    GetHAL().pushCanvas();
}

void AppKeyboard::update_tiktok_controller()
{
    bool mouse_connected = GetHAL().bleMouseIsConnected();

    // Check if connection status changed
    if (mouse_connected != _last_mouse_connected) {
        _last_mouse_connected = mouse_connected;
        
        if (mouse_connected && !_tiktok_mouse_positioned) {
            // Move cursor to top-left corner first
            mclog::tagInfo(getAppInfo().name, "TikTok: positioning cursor to top-left");
            GetHAL().bleMouseMove(-9999, -9999);
            delay(50);
            // Then move 200 points down and right to be in work area
            mclog::tagInfo(getAppInfo().name, "TikTok: moving cursor to work area (+200, +200)");
            GetHAL().bleMouseMove(200, 200);
            _tiktok_mouse_positioned = true;
        }
        
        render_tiktok_interface();
    }

    // Update UI periodically
    if (GetHAL().millis() - _info_update_time > 1000) {
        render_tiktok_interface();
        _info_update_time = GetHAL().millis();
    }

    auto event = GetHAL().keyboard.getLatestKeyEventRaw();
    
    // No key event
    if (event.row == 0 && event.col == 0) {
        return;
    }

    if (!mouse_connected) {
        return;
    }

    if (event.state) {
        // Key pressed - simulate swipe by pressing and dragging
        // Up arrow (row=2, col=11)
        if (event.row == 2 && event.col == 11) {
            mclog::tagInfo(getAppInfo().name, "TikTok: swipe up (next video)");
            GetHAL().bleMousePress(MOUSE_LEFT);
            delay(20);
            GetHAL().bleMouseMove(0, -200);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_LEFT);
            // Return cursor to original position
            delay(20);
            GetHAL().bleMouseMove(0, 200);
        }
        // Down arrow (row=3, col=11)
        else if (event.row == 3 && event.col == 11) {
            mclog::tagInfo(getAppInfo().name, "TikTok: swipe down (prev video)");
            GetHAL().bleMousePress(MOUSE_LEFT);
            delay(20);
            GetHAL().bleMouseMove(0, 200);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_LEFT);
            // Return cursor to original position
            delay(20);
            GetHAL().bleMouseMove(0, -200);
        }
        // Left arrow (row=3, col=10)
        else if (event.row == 3 && event.col == 10) {
            mclog::tagInfo(getAppInfo().name, "TikTok: swipe left");
            GetHAL().bleMousePress(MOUSE_LEFT);
            delay(20);
            GetHAL().bleMouseMove(-200, 0);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_LEFT);
            // Return cursor to original position
            delay(20);
            GetHAL().bleMouseMove(200, 0);
        }
        // Right arrow (row=3, col=12)
        else if (event.row == 3 && event.col == 12) {
            mclog::tagInfo(getAppInfo().name, "TikTok: swipe right");
            GetHAL().bleMousePress(MOUSE_LEFT);
            delay(20);
            GetHAL().bleMouseMove(200, 0);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_LEFT);
            // Return cursor to original position
            delay(20);
            GetHAL().bleMouseMove(-200, 0);
        }
        // Enter key (row=2, col=13) - double click for like
        else if (event.row == 2 && event.col == 13) {
            mclog::tagInfo(getAppInfo().name, "TikTok: double click (like)");
            GetHAL().bleMousePress(MOUSE_LEFT);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_LEFT);
            delay(50);
            GetHAL().bleMousePress(MOUSE_LEFT);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_LEFT);
        }
        // row=3, col=13 — пауза (ПКМ)
        else if (event.row == 3 && event.col == 13) {
            mclog::tagInfo(getAppInfo().name, "TikTok: pause (right click)");
            GetHAL().bleMousePress(MOUSE_RIGHT);
            delay(20);
            GetHAL().bleMouseRelease(MOUSE_RIGHT);
        }
        // row=2, col=12 — возврат курсора в центр
        else if (event.row == 2 && event.col == 12) {
            mclog::tagInfo(getAppInfo().name, "TikTok: move cursor to center");
            // Пример: переместить в центр (400, 120) — подберите под ваш экран
            GetHAL().bleMouseMove(-99999, -99999); // в левый верхний угол
            delay(20);
            GetHAL().bleMouseMove(400, 120); // смещение в центр
        }
    }
}
