/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <keyboard/keyboard.h>
#include <cstdint>
#include <string>

class AppBleHid : public mooncake::AppAbility {
public:
    AppBleHid();
    ~AppBleHid();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class ActionType {
        None,
        ToggleKeyboard,
        ToggleMouse,
        StartAdvertising,
        ClearBonding,
        MediaPlayPause,
        MediaPrev,
        MediaNext,
        TikTokDrag,
        MouseCenter,
        KickClient,
    };

    enum class ScreenType {
        Main,
        Keyboard,
        Media,
        Mouse,
        TikTok,
    };

    void handle_key_event(const Keyboard::KeyEvent_t& keyEvent);
    void handle_key_event_raw(const Keyboard::KeyEventRaw_t& keyEvent);
    void render_interface();
    void render_main();
    void render_keyboard();
    void render_media();
    void render_mouse();
    void render_tiktok();
    void set_status(const std::string& text);
    void toggle_ble_keyboard();
    void toggle_ble_mouse();
    void media_tap(uint16_t usage_id);
    void tiktok_drag_down();

    int _key_event_slot_id = -1;
    int _key_event_raw_slot_id = -1;
    bool _ble_keyboard_on = false;
    bool _ble_mouse_on = false;
    bool _last_kb_connected = false;
    bool _last_mouse_connected = false;
    ActionType _pending_action = ActionType::None;
    ScreenType _screen = ScreenType::Main;
    int _menu_index = 0;
    std::string _status;
};
