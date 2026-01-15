/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <hal/hal.h>
#include <string>
#include <vector>
#include <smooth_ui_toolkit.h>

/**
 * @brief
 *
 */
class KeyboardSelectorMenu : public smooth_ui_toolkit::SmoothSelectorMenu {
public:
    enum KeyboardType_t {
        KEYBOARD_TYPE_NONE = 0,
        KEYBOARD_TYPE_BLE,
        KEYBOARD_TYPE_USB,
        KEYBOARD_TYPE_TIKTOK,
        KEYBOARD_TYPE_MEDIA,
    };

    void init();
    void onReadInput() override;
    void onRender() override;
    void onClick() override;

    bool isSelected() const
    {
        return _is_selected;
    }

    KeyboardType_t getSelectedType() const
    {
        return _keyboard_type;
    }

private:
    bool _is_selected             = false;
    KeyboardType_t _keyboard_type = KEYBOARD_TYPE_NONE;
};

/**
 * @brief
 *
 */
class AppKeyboard : public mooncake::AppAbility {
public:
    AppKeyboard();
    ~AppKeyboard();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    uint32_t _info_update_time           = 0;
    bool _is_selecting                   = true;
    bool _is_keyboard_active             = false;
    bool _last_ble_connected             = false;
    bool _last_usb_connected             = false;
    KeyboardSelectorMenu::KeyboardType_t _keyboard_type = KeyboardSelectorMenu::KEYBOARD_TYPE_NONE;
    bool _show_restart_confirm           = false;
    bool _restart_confirm_dirty          = false;
    int _restart_confirm_choice          = 0;
    KeyboardSelectorMenu* _selector_menu = nullptr;
    bool _is_tiktok_mode                 = false;
    bool _is_media_mode                  = false;
    bool _tiktok_mouse_positioned        = false;
    bool _last_mouse_connected           = false;
    bool _show_help_menu                 = false;
    // Debounce for BLE keyboard connection reporting
    bool _ble_candidate_state            = false;
    uint32_t _ble_candidate_time         = 0;
    const uint32_t _ble_debounce_ms      = 1500;
    // Debounce for BLE Mouse connection reporting
    bool _ble_mouse_candidate_state      = false;
    uint32_t _ble_mouse_candidate_time   = 0;
    bool _ble_mouse_stable_connected     = false;

    void select_keyboard_type();
    void init_ble_keyboard();
    void init_usb_keyboard();
    void init_tiktok_controller();
    void init_media_controller();
    void update_media_controller();
    void render_media_interface();
    void update_tiktok_controller();
    void render_tiktok_interface();
    void update_connection_info();
    void render_keyboard_interface();
    void render_connection_status();
    void render_restart_confirm();
    void update_restart_confirm();
};
