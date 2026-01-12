/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include <smooth_ui_toolkit.h>
#include <M5Unified.hpp>
#include <mooncake_log.h>
#include <mooncake.h>
#include <apps.h>
#include <hal.h>
#include <algorithm>

using namespace mooncake;
using namespace smooth_ui_toolkit;

static bool g_run_update = false;

void setup()
{
    // Setup logger
    mclog::set_level(mclog::level_debug);
    mclog::set_time_format(mclog::time_format_unix_milliseconds);

    // HAL init
    GetHAL().init();

    // Setup ui hal
    ui_hal::on_delay([](uint32_t ms) { GetHAL().delay(ms); });
    ui_hal::on_get_tick([]() { return GetHAL().millis(); });

    // Install apps
    GetMooncake().installApp(std::make_unique<Launcher>());
    GetMooncake().installApp(std::make_unique<AppWifiScan>());
    GetMooncake().installApp(std::make_unique<AppRecord>());
    GetMooncake().installApp(std::make_unique<AppChat>());
    GetMooncake().installApp(std::make_unique<AppRemote>());
    GetMooncake().installApp(std::make_unique<AppSetWiFi>());
    GetMooncake().installApp(std::make_unique<AppSettingsMenu>());
    GetMooncake().installApp(std::make_unique<AppSettings>());
    GetMooncake().installApp(std::make_unique<AppDisplay>());
    GetMooncake().installApp(std::make_unique<AppClock>());
    GetMooncake().installApp(std::make_unique<AppKeyboard>());
    GetMooncake().installApp(std::make_unique<AppImu>());
    GetMooncake().installApp(std::make_unique<AppSdcard>());
    GetMooncake().installApp(std::make_unique<AppStringIRToolKit>());
    GetMooncake().installApp(std::make_unique<AppLoraChat>());
    GetMooncake().installApp(std::make_unique<AppGPS>());
    // GetMooncake().installApp(std::make_unique<AppDummy>());

    // Main loop
    audio::load_keyboard_sfx_settings();
    {
        int32_t bright = GetHAL().getSettings().GetInt("disp_bright", 10);
        bright         = std::clamp<int32_t>(bright, 0, 10);
        uint8_t hw_brightness = static_cast<uint8_t>((bright * 255 + 5) / 10);
        GetHAL().display.setBrightness(hw_brightness);
    }
    {
        bool kb_ind = GetHAL().getSettings().GetInt("disp_kb_ind", 1) != 0;
        GetHAL().setKeyboardBarVisible(kb_ind);
    }
    g_run_update = true;
}

void loop()
{
    if (!g_run_update) {
        delay(10);
        return;
    }

    GetHAL().feedTheDog();
    GetHAL().update();
    GetMooncake().update();
}
