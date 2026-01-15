/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <hal/hal.h>

class AppSettingsMenu : public mooncake::AppAbility {
public:
    AppSettingsMenu();
    ~AppSettingsMenu();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    class SettingsSelectorMenu;
    SettingsSelectorMenu* _menu = nullptr;

    int _wifi_app_id       = -1;
    int _sound_app_id      = -1;
    int _display_app_id    = -1;
    int _child_app_id      = -1;
    bool _is_suspended     = false;

    // System bar debounce
    uint32_t _system_bar_update_time = 0;
    uint32_t _system_bar_update_period = 1000; // ms
    bool _last_wifi_connected = false;

    void resolve_app_ids();
};
