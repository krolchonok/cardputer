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
    int _child_app_id      = -1;
    bool _is_suspended     = false;

    void resolve_app_ids();
};
