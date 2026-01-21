/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ble_hid.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <hal/hal.h>

using namespace mooncake;

namespace {
constexpr uint16_t kHidConsumerPlayPause = 0x00CD;
constexpr uint16_t kHidConsumerScanNext = 0x00B5;
constexpr uint16_t kHidConsumerScanPrev = 0x00B6;
constexpr uint8_t kMouseButtonLeft = 0x01;
}  // namespace

AppBleHid::AppBleHid()
{
    setAppInfo().name = "BLE HID";
}

AppBleHid::~AppBleHid()
{
}

void AppBleHid::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _status.clear();
    _ble_keyboard_on = GetHAL().bleKeyboardIsInited();
    _ble_mouse_on = GetHAL().bleMouseIsInited();
    _last_kb_connected = _ble_keyboard_on && GetHAL().bleKeyboardIsConnected();
    _last_mouse_connected = _ble_mouse_on && GetHAL().bleMouseIsConnected();
    _screen = ScreenType::Main;
    _menu_index = 0;

    _key_event_slot_id = GetHAL().keyboard.onKeyEvent.connect(
        [this](const Keyboard::KeyEvent_t& keyEvent) { handle_key_event(keyEvent); });
    _key_event_raw_slot_id = GetHAL().keyboard.onKeyEventRaw.connect(
        [this](const Keyboard::KeyEventRaw_t& keyEvent) { handle_key_event_raw(keyEvent); });

    render_interface();
}

void AppBleHid::onRunning()
{
    if (_pending_action != ActionType::None) {
        ActionType action = _pending_action;
        _pending_action = ActionType::None;
        switch (action) {
            case ActionType::ToggleKeyboard:
                toggle_ble_keyboard();
                break;
            case ActionType::ToggleMouse:
                toggle_ble_mouse();
                break;
            case ActionType::StartAdvertising:
                if (_ble_keyboard_on) {
                    GetHAL().bleKeyboardSetAutoAdvertise(true);
                    GetHAL().bleKeyboardSetAllowConnections(true);
                    GetHAL().bleKeyboardStartAdvertising();
                    set_status("Keyboard advertising");
                } else {
                    set_status("Keyboard is off");
                }
                break;
            case ActionType::ClearBonding:
                if (_ble_keyboard_on) {
                    GetHAL().bleKeyboardSetAutoAdvertise(false);
                    GetHAL().bleKeyboardSetAllowConnections(false);
                    GetHAL().bleKeyboardStopAdvertising();
                    GetHAL().bleKeyboardClearBonding();
                    bool rotated = GetHAL().bleKeyboardRotateAddress();
                    if (rotated) {
                        GetHAL().getSettings().SetString("ble_rand_addr", GetHAL().bleKeyboardGetAddressString());
                    }
                    set_status(rotated ? "Bond cleared; addr rotated" : "Bond cleared; addr failed");
                } else {
                    set_status("Keyboard is off");
                }
                break;
            case ActionType::MediaPlayPause:
                media_tap(kHidConsumerPlayPause);
                break;
            case ActionType::MediaPrev:
                media_tap(kHidConsumerScanPrev);
                break;
            case ActionType::MediaNext:
                media_tap(kHidConsumerScanNext);
                break;
            case ActionType::TikTokDrag:
                tiktok_drag_down();
                break;
            case ActionType::MouseCenter:
                if (_ble_mouse_on) {
                    GetHAL().bleMouseCenterCursor();
                    set_status("Mouse centered");
                } else {
                    set_status("Mouse is off");
                }
                break;
            case ActionType::KickClient:
                if (_ble_keyboard_on && GetHAL().bleKeyboardIsConnected()) {
                    GetHAL().bleKeyboardDisconnect();
                    GetHAL().bleKeyboardSetAllowConnections(false);
                    GetHAL().bleKeyboardSetAutoAdvertise(false);
                    GetHAL().bleKeyboardStopAdvertising();
                    set_status("Disconnected; use Fn+2 to advertise");
                } else {
                    set_status("No client connected");
                }
                break;
            case ActionType::None:
                break;
        }
    }

    _ble_keyboard_on = GetHAL().bleKeyboardIsInited();
    _ble_mouse_on = GetHAL().bleMouseIsInited();
    const bool kb_connected = _ble_keyboard_on && GetHAL().bleKeyboardIsConnected();
    const bool mouse_connected = _ble_mouse_on && GetHAL().bleMouseIsConnected();
    if (kb_connected != _last_kb_connected || mouse_connected != _last_mouse_connected) {
        _last_kb_connected = kb_connected;
        _last_mouse_connected = mouse_connected;
        render_interface();
    }

    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        if (_screen != ScreenType::Main) {
            _screen = ScreenType::Main;
            GetHAL().bleKeyboardDisconnect();
            if (GetHAL().bleMouseIsInited()) {
                GetHAL().bleMouseDeinit();
            }
            render_interface();
        } else {
            close();
        }
    }
}

void AppBleHid::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_key_event_slot_id >= 0) {
        GetHAL().keyboard.onKeyEvent.disconnect(_key_event_slot_id);
        _key_event_slot_id = -1;
    }
    if (_key_event_raw_slot_id >= 0) {
        GetHAL().keyboard.onKeyEventRaw.disconnect(_key_event_raw_slot_id);
        _key_event_raw_slot_id = -1;
    }

    GetHAL().bleKeyboardDisconnect();
    if (GetHAL().bleMouseIsInited()) {
        GetHAL().bleMouseDeinit();
    }
}

void AppBleHid::handle_key_event(const Keyboard::KeyEvent_t& keyEvent)
{
    if (!keyEvent.state || keyEvent.isModifier) {
        return;
    }

    if (_pending_action != ActionType::None) {
        return;
    }

    const bool fn_active = GetHAL().keyboard.isFnActive();

    if (_screen == ScreenType::Main) {
        if (keyEvent.keyCode == KEY_UP) {
            _menu_index = (_menu_index + 3) % 4;
            render_interface();
            return;
        }
        if (keyEvent.keyCode == KEY_DOWN) {
            _menu_index = (_menu_index + 1) % 4;
            render_interface();
            return;
        }
        if (keyEvent.keyCode == KEY_ENTER) {
            switch (_menu_index) {
                case 0:
                    _screen = ScreenType::Keyboard;
                    break;
                case 1:
                    _screen = ScreenType::Media;
                    break;
                case 2:
                    _screen = ScreenType::Mouse;
                    break;
                case 3:
                    _screen = ScreenType::TikTok;
                    break;
                default:
                    break;
            }
            render_interface();
            return;
        }
        return;
    }

    if (!fn_active) {
        return;
    }

    switch (_screen) {
        case ScreenType::Keyboard:
            if (keyEvent.keyCode == KEY_1) {
                _pending_action = ActionType::ToggleKeyboard;
            } else if (keyEvent.keyCode == KEY_2) {
                _pending_action = ActionType::StartAdvertising;
            } else if (keyEvent.keyCode == KEY_3) {
                _pending_action = ActionType::ClearBonding;
            } else if (keyEvent.keyCode == KEY_4) {
                bool next = !GetHAL().bleKeyboardGetAutoAdvertise();
                GetHAL().bleKeyboardSetAutoAdvertise(next);
                set_status(next ? "Auto adv on" : "Auto adv off");
            } else if (keyEvent.keyCode == KEY_5) {
                _pending_action = ActionType::KickClient;
            }
            break;
        case ScreenType::Media:
            if (keyEvent.keyCode == KEY_1) {
                _pending_action = ActionType::MediaPlayPause;
            } else if (keyEvent.keyCode == KEY_2) {
                _pending_action = ActionType::MediaPrev;
            } else if (keyEvent.keyCode == KEY_3) {
                _pending_action = ActionType::MediaNext;
            }
            break;
        case ScreenType::Mouse:
            if (keyEvent.keyCode == KEY_1) {
                _pending_action = ActionType::ToggleMouse;
            } else if (keyEvent.keyCode == KEY_2) {
                _pending_action = ActionType::MouseCenter;
            }
            break;
        case ScreenType::TikTok:
            if (keyEvent.keyCode == KEY_1) {
                _pending_action = ActionType::ToggleMouse;
            } else if (keyEvent.keyCode == KEY_2) {
                _pending_action = ActionType::TikTokDrag;
            }
            break;
        case ScreenType::Main:
            break;
    }
}

void AppBleHid::handle_key_event_raw(const Keyboard::KeyEventRaw_t& keyEvent)
{
    if (!keyEvent.state) {
        return;
    }

    const bool is_up = (keyEvent.row == 2 && keyEvent.col == 11);
    const bool is_down = (keyEvent.row == 3 && keyEvent.col == 11);
    const bool is_right = (keyEvent.row == 3 && keyEvent.col == 12);
    const bool is_enter = (keyEvent.row == 2 && keyEvent.col == 13);

    if (_screen == ScreenType::Main) {
        if (is_up) {
            _menu_index = (_menu_index + 3) % 4;
            render_interface();
            return;
        }
        if (is_down) {
            _menu_index = (_menu_index + 1) % 4;
            render_interface();
            return;
        }
        if (is_enter || is_right) {
            switch (_menu_index) {
                case 0:
                    _screen = ScreenType::Keyboard;
                    break;
                case 1:
                    _screen = ScreenType::Media;
                    break;
                case 2:
                    _screen = ScreenType::Mouse;
                    break;
                case 3:
                    _screen = ScreenType::TikTok;
                    break;
                default:
                    break;
            }
            render_interface();
            return;
        }
        return;
    }

}

void AppBleHid::render_interface()
{
    switch (_screen) {
        case ScreenType::Main:
            render_main();
            break;
        case ScreenType::Keyboard:
            render_keyboard();
            break;
        case ScreenType::Media:
            render_media();
            break;
        case ScreenType::Mouse:
            render_mouse();
            break;
        case ScreenType::TikTok:
            render_tiktok();
            break;
    }

    GetHAL().pushCanvas();
}

void AppBleHid::render_main()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.println("BLE HID");

    const char* items[] = {"Keyboard", "Media", "Mouse", "TikTok"};
    const int item_count = 4;
    const int row_h = FONT_HEIGHT;
    const int start_y = 16;

    for (int i = 0; i < item_count; ++i) {
        int y = start_y + i * row_h;
        if (i == _menu_index) {
            GetHAL().canvas.fillRect(0, y - 1, GetHAL().canvas.width(), row_h + 2, THEME_COLOR_ICON);
            GetHAL().canvas.setTextColor(TFT_BLACK, THEME_COLOR_ICON);
        } else {
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
        }
        GetHAL().canvas.setCursor(2, y);
        GetHAL().canvas.print(items[i]);
    }

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 80);
    GetHAL().canvas.print("Up/Down select, Enter open");
}

void AppBleHid::render_keyboard()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.println("Status");

    const bool kb_connected = _ble_keyboard_on && GetHAL().bleKeyboardIsConnected();
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 16);
    GetHAL().canvas.printf("Status: %s  Conn: %s",
                           _ble_keyboard_on ? "On" : "Off",
                           kb_connected ? "Yes" : "No");
    GetHAL().canvas.setCursor(0, 32);
    GetHAL().canvas.printf("BLE MAC: %s", GetHAL().bleKeyboardGetAddressString().c_str());

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 48);
    GetHAL().canvas.print("Fn+1 Toggle  Fn+2 Adv");
    GetHAL().canvas.setCursor(0, 64);
    GetHAL().canvas.print("Fn+3 Clear  Fn+4 Auto");
    GetHAL().canvas.setCursor(0, 80);
    GetHAL().canvas.print("Fn+5 Disconnect");

    if (!_status.empty()) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.setCursor(0, 96);
        GetHAL().canvas.print(_status.c_str());
    }
}

void AppBleHid::render_media()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.println("Media");

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 16);
    GetHAL().canvas.print("Fn+1 Play/Pause");
    GetHAL().canvas.setCursor(0, 32);
    GetHAL().canvas.print("Fn+2 Previous");
    GetHAL().canvas.setCursor(0, 48);
    GetHAL().canvas.print("Fn+3 Next");
    GetHAL().canvas.setCursor(0, 64);
    GetHAL().canvas.print("Need keyboard on");

    if (!_status.empty()) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.setCursor(0, 96);
        GetHAL().canvas.print(_status.c_str());
    }
}

void AppBleHid::render_mouse()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.println("Mouse");

    const bool mouse_connected = _ble_mouse_on && GetHAL().bleMouseIsConnected();
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 16);
    GetHAL().canvas.printf("Status: %s", _ble_mouse_on ? "On" : "Off");
    GetHAL().canvas.setCursor(0, 32);
    GetHAL().canvas.printf("Connected: %s", mouse_connected ? "Yes" : "No");

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 48);
    GetHAL().canvas.print("Fn+1 Toggle");
    GetHAL().canvas.setCursor(0, 64);
    GetHAL().canvas.print("Fn+2 Center");

    if (!_status.empty()) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.setCursor(0, 96);
        GetHAL().canvas.print(_status.c_str());
    }
}

void AppBleHid::render_tiktok()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.println("TikTok");

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 16);
    GetHAL().canvas.print("Fn+1 Toggle mouse");
    GetHAL().canvas.setCursor(0, 32);
    GetHAL().canvas.print("Fn+2 Drag down");

    if (!_status.empty()) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.setCursor(0, 64);
        GetHAL().canvas.print(_status.c_str());
    }
}

void AppBleHid::set_status(const std::string& text)
{
    _status = text;
    render_interface();
}

void AppBleHid::toggle_ble_keyboard()
{
    if (_ble_keyboard_on) {
        GetHAL().bleKeyboardDeinit();
        GetHAL().delay(120);
        _ble_keyboard_on = false;
        set_status("Keyboard off");
        return;
    }

    if (_ble_mouse_on) {
        GetHAL().bleMouseDeinit();
        _ble_mouse_on = false;
        GetHAL().delay(120);
    }

    GetHAL().bleKeyboardInit();
    GetHAL().bleKeyboardSetAutoAdvertise(true);
    GetHAL().bleKeyboardSetAllowConnections(true);
    _ble_keyboard_on = true;
    set_status("Keyboard on");
}

void AppBleHid::toggle_ble_mouse()
{
    if (_ble_mouse_on) {
        GetHAL().bleMouseDeinit();
        GetHAL().delay(120);
        _ble_mouse_on = false;
        set_status("Mouse off");
        return;
    }

    if (_ble_keyboard_on) {
        GetHAL().bleKeyboardDeinit();
        _ble_keyboard_on = false;
        GetHAL().delay(120);
    }

    GetHAL().bleMouseInit();
    _ble_mouse_on = true;
    set_status("Mouse on");
}

void AppBleHid::media_tap(uint16_t usage_id)
{
    if (!_ble_keyboard_on) {
        set_status("Keyboard is off");
        return;
    }

    GetHAL().bleKeyboardSendMediaKey(usage_id, true);
    GetHAL().delay(10);
    GetHAL().bleKeyboardSendMediaKey(usage_id, false);
    set_status("Media key sent");
}

void AppBleHid::tiktok_drag_down()
{
    if (!_ble_mouse_on || !GetHAL().bleMouseIsConnected()) {
        set_status("Mouse not connected");
        return;
    }

    GetHAL().bleMousePress(kMouseButtonLeft);
    for (int i = 0; i < 12; ++i) {
        GetHAL().bleMouseMove(0, 8);
        GetHAL().delay(12);
    }
    GetHAL().bleMouseRelease(kMouseButtonLeft);
    set_status("TikTok drag");
}
