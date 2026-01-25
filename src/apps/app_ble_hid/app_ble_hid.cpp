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
#include <hal/utils/ble_keyboard_wrapper/ble_keyboard_wrapper.h>
#include <esp_system.h>

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
    _current_profile = GetHAL().getSettings().GetInt("ble_profile", 0);

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
            case ActionType::AutoStartKeyboard:
                // Auto-start keyboard and advertising when entering Keyboard screen
                if (!_ble_keyboard_on) {
                    load_current_profile();
                    toggle_ble_keyboard();
                }
                if (_ble_keyboard_on && !GetHAL().bleKeyboardIsConnected()) {
                    GetHAL().bleKeyboardSetAutoAdvertise(true);
                    GetHAL().bleKeyboardSetAllowConnections(true);
                    GetHAL().bleKeyboardStartAdvertising();
                    set_status("Profile " + std::to_string(_current_profile + 1) + " - Advertising...");
                }
                break;
            case ActionType::AutoStartMedia:
                // Auto-start keyboard for media controls
                if (!_ble_keyboard_on) {
                    load_current_profile();
                    toggle_ble_keyboard();
                }
                if (_ble_keyboard_on && !GetHAL().bleKeyboardIsConnected()) {
                    GetHAL().bleKeyboardSetAutoAdvertise(true);
                    GetHAL().bleKeyboardSetAllowConnections(true);
                    GetHAL().bleKeyboardStartAdvertising();
                    set_status("Profile " + std::to_string(_current_profile + 1) + " - Advertising...");
                }
                break;
            case ActionType::AutoStartMouse:
                // Auto-start mouse when entering Mouse screen
                if (!_ble_mouse_on) {
                    if (_ble_keyboard_on) {
                        GetHAL().bleKeyboardDeinit();
                        _ble_keyboard_on = false;
                        GetHAL().delay(120);
                    }
                    GetHAL().bleMouseInit();
                    _ble_mouse_on = true;
                    set_status("Mouse started");
                }
                break;
            case ActionType::SwitchProfile1:
                switch_profile(0);
                break;
            case ActionType::SwitchProfile2:
                switch_profile(1);
                break;
            case ActionType::SwitchProfile3:
                switch_profile(2);
                break;
            case ActionType::SwitchProfile4:
                switch_profile(3);
                break;
            case ActionType::SwitchProfile5:
                switch_profile(4);
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
                    _pending_action = ActionType::AutoStartKeyboard;
                    break;
                case 1:
                    _screen = ScreenType::Media;
                    _pending_action = ActionType::AutoStartMedia;
                    break;
                case 2:
                    _screen = ScreenType::Mouse;
                    _pending_action = ActionType::AutoStartMouse;
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
            } else if (keyEvent.keyCode == KEY_6) {
                _pending_action = ActionType::SwitchProfile1;
            } else if (keyEvent.keyCode == KEY_7) {
                _pending_action = ActionType::SwitchProfile2;
            } else if (keyEvent.keyCode == KEY_8) {
                _pending_action = ActionType::SwitchProfile3;
            } else if (keyEvent.keyCode == KEY_9) {
                _pending_action = ActionType::SwitchProfile4;
            } else if (keyEvent.keyCode == KEY_0) {
                _pending_action = ActionType::SwitchProfile5;
            }
            break;
        case ScreenType::Media:
            if (keyEvent.keyCode == KEY_1) {
                _pending_action = ActionType::MediaPlayPause;
            } else if (keyEvent.keyCode == KEY_2) {
                _pending_action = ActionType::MediaPrev;
            } else if (keyEvent.keyCode == KEY_3) {
                _pending_action = ActionType::MediaNext;
            } else if (keyEvent.keyCode == KEY_6) {
                _pending_action = ActionType::SwitchProfile1;
            } else if (keyEvent.keyCode == KEY_7) {
                _pending_action = ActionType::SwitchProfile2;
            } else if (keyEvent.keyCode == KEY_8) {
                _pending_action = ActionType::SwitchProfile3;
            } else if (keyEvent.keyCode == KEY_9) {
                _pending_action = ActionType::SwitchProfile4;
            } else if (keyEvent.keyCode == KEY_0) {
                _pending_action = ActionType::SwitchProfile5;
            }
            break;
        case ScreenType::Mouse:
            if (keyEvent.keyCode == KEY_1) {
                _pending_action = ActionType::ToggleMouse;
            } else if (keyEvent.keyCode == KEY_2) {
                _pending_action = ActionType::MouseCenter;
            } else if (keyEvent.keyCode == KEY_6) {
                _pending_action = ActionType::SwitchProfile1;
            } else if (keyEvent.keyCode == KEY_7) {
                _pending_action = ActionType::SwitchProfile2;
            } else if (keyEvent.keyCode == KEY_8) {
                _pending_action = ActionType::SwitchProfile3;
            } else if (keyEvent.keyCode == KEY_9) {
                _pending_action = ActionType::SwitchProfile4;
            } else if (keyEvent.keyCode == KEY_0) {
                _pending_action = ActionType::SwitchProfile5;
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
                    _pending_action = ActionType::AutoStartKeyboard;
                    break;
                case 1:
                    _screen = ScreenType::Media;
                    _pending_action = ActionType::AutoStartMedia;
                    break;
                case 2:
                    _screen = ScreenType::Mouse;
                    _pending_action = ActionType::AutoStartMouse;
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
    GetHAL().canvas.printf("Profile %d", _current_profile + 1);

    const bool kb_connected = _ble_keyboard_on && GetHAL().bleKeyboardIsConnected();
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 12);
    GetHAL().canvas.printf("%s  Conn: %s",
                           _ble_keyboard_on ? "On" : "Off",
                           kb_connected ? "Yes" : "No");
    GetHAL().canvas.setCursor(0, 24);
    GetHAL().canvas.printf("MAC: %s", GetHAL().bleKeyboardGetAddressString().c_str());

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 40);
    GetHAL().canvas.print("Fn+1 On/Off  Fn+2 Adv  Fn+3 Clear");
    GetHAL().canvas.setCursor(0, 52);
    GetHAL().canvas.print("Fn+4 AutoAdv  Fn+5 Disconnect");
    GetHAL().canvas.setCursor(0, 68);
    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.print("Fn+6..0 = Profile 1..5");

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
    GetHAL().canvas.printf("Media - Profile %d", _current_profile + 1);

    const bool kb_connected = _ble_keyboard_on && GetHAL().bleKeyboardIsConnected();
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 12);
    GetHAL().canvas.printf("%s  Conn: %s",
                           _ble_keyboard_on ? "On" : "Off",
                           kb_connected ? "Yes" : "No");

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 28);
    GetHAL().canvas.print("Fn+1 Play/Pause  Fn+2 Prev  Fn+3 Next");
    GetHAL().canvas.setCursor(0, 44);
    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.print("Fn+6..0 = Profile 1..5");

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
    GetHAL().canvas.printf("Mouse - Profile %d", _current_profile + 1);

    const bool mouse_connected = _ble_mouse_on && GetHAL().bleMouseIsConnected();
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 12);
    GetHAL().canvas.printf("%s  Conn: %s",
                           _ble_mouse_on ? "On" : "Off",
                           mouse_connected ? "Yes" : "No");

    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, 28);
    GetHAL().canvas.print("Fn+1 Toggle  Fn+2 Center");
    GetHAL().canvas.setCursor(0, 44);
    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.print("Fn+6..0 = Profile 1..5");

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

void AppBleHid::generate_profile_address(int profile, uint8_t* addr)
{
    // Generate a unique static random address based on device MAC and profile number
    uint64_t mac64 = ESP.getEfuseMac();
    
    // Mix profile into the address
    for (int i = 0; i < 6; i++) {
        addr[i] = ((mac64 >> (8 * i)) & 0xFF) ^ (profile * 0x11 + i * 0x23);
    }
    
    // Set two MSBs of addr[5] to 11 for static random address (BLE spec)
    addr[5] = (addr[5] & 0x3F) | 0xC0;
}

void AppBleHid::load_current_profile()
{
    // Load or generate address for current profile
    std::string key = "ble_prof_" + std::to_string(_current_profile);
    std::string saved_addr = GetHAL().getSettings().GetString(key, "");
    
    uint8_t addr[6] = {0};
    bool valid = false;
    
    if (saved_addr.size() == 17) {
        // Parse saved address
        int parsed = 0;
        for (int i = 0; i < 6; ++i) {
            char hi = saved_addr[i * 3];
            char lo = saved_addr[i * 3 + 1];
            if (saved_addr[i * 3 + 2] != ':' && i != 5) break;
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi_n = hex(hi);
            int lo_n = hex(lo);
            if (hi_n < 0 || lo_n < 0) break;
            addr[i] = static_cast<uint8_t>((hi_n << 4) | lo_n);
            parsed++;
        }
        valid = (parsed == 6);
    }
    
    if (!valid) {
        // Generate new address for this profile
        generate_profile_address(_current_profile, addr);
        
        // Save it
        char addr_str[18];
        snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        GetHAL().getSettings().SetString(key, addr_str);
    }
    
    // Set the address for BLE wrapper to use
    ble_keyboard_wrapper_set_saved_address(addr);
}

void AppBleHid::switch_profile(int profile)
{
    if (profile < 0 || profile >= kNumProfiles) {
        return;
    }
    
    // For Mouse screen, just update profile (mouse doesn't have profile addresses yet)
    if (_screen == ScreenType::Mouse) {
        _current_profile = profile;
        GetHAL().getSettings().SetInt("ble_profile", profile);
        set_status("Profile " + std::to_string(profile + 1) + " selected");
        render_interface();
        return;
    }
    
    // For Keyboard and Media screens
    if (profile == _current_profile && _ble_keyboard_on) {
        set_status("Already on Profile " + std::to_string(profile + 1));
        return;
    }
    
    // Save new profile selection
    _current_profile = profile;
    GetHAL().getSettings().SetInt("ble_profile", profile);
    
    // Generate and save address for this profile if not exists
    load_current_profile();
    
    // Show reboot message
    _status = "Rebooting to Profile " + std::to_string(profile + 1) + "...";
    render_interface();
    GetHAL().pushCanvas();
    
    // Set flag to auto-open BLE HID after reboot
    GetHAL().getSettings().SetInt("ble_auto", 1);
    
    GetHAL().delay(500);
    
    // Reboot to apply new profile (ESP32 BLE stack doesn't reliably reinit)
    ESP.restart();
}
