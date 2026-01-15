/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_launcher.h"
#include <apps/utils/theme.h>
#include <apps/utils/common.h>
#include <mooncake_log.h>
#include <hal/hal.h>
#include <hal.h>
#include <algorithm>

using namespace mooncake;

void Launcher::onCreate()
{
    setAppInfo().name = "Launcher";
    mclog::tagInfo(getAppInfo().name, "on create");

    // Init
    // Skip boot animation to show menu immediately and apply saved brightness
    {
        int32_t bright = GetHAL().getSettings().GetInt("disp_bright", 10);
        bright = std::clamp<int32_t>(bright, 0, 10);
        uint8_t hw_brightness = static_cast<uint8_t>((bright * 255 + 5) / 10);
        GetHAL().display.setBrightness(hw_brightness);
    }
    start_menu();
    start_system_bar();
    start_keyboard_bar();

    open();
}

void Launcher::onRunning()
{
    // mclog::tagInfo(getAppInfo().name, "on running");

    bool visible = GetHAL().getSettings().GetInt("disp_kb_ind", 1) != 0;
    if (visible != _data.keyboard_bar_visible) {
        _data.keyboard_bar_visible = visible;
        render_keyboard_bar();
    }

    update_system_bar();

    // If app is opened and running
    if (_data.running_app_id >= 0) {
        // If running app is closed
        if (GetMooncake().getAppCurrentState(_data.running_app_id) == AppAbility::StateSleeping) {
            _data.running_app_id = -1;
            update_menu(true);  // Render menu first
            ANIM_APP_CLOSE();
        }
    } else {
        update_menu();
    }
}
