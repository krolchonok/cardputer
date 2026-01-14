/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_usb_storage.h"
#include "assets/usb_big.h"
#include "assets/usb_small.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <hal/utils/usb_msc/usb_msc_helper.h>
#include <mooncake_log.h>

using namespace mooncake;

AppUsbStorage::AppUsbStorage()
{
    setAppInfo().name     = "USB Storage";
    setAppInfo().userData = new AppIcon_t(image_data_usb_big, image_data_usb_small);
}

AppUsbStorage::~AppUsbStorage()
{
    delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppUsbStorage::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    GetHAL().canvas.setBaseColor(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextScroll(false);
    GetHAL().canvas.setTextSize(1);

    _msc_active = usb_msc_helper_is_active();
    _last_update = GetHAL().millis();

    render();
}

void AppUsbStorage::onRunning()
{
    auto key_event = GetHAL().keyboard.getLatestKeyEvent();
    if (key_event.state && key_event.keyCode == KEY_ENTER) {
        audio::play_random_tone();
        toggle_msc();
        GetHAL().delay(200);
    }

    // Auto update display every 500ms
    if (GetHAL().millis() - _last_update > 500) {
        _last_update = GetHAL().millis();
        render();
    }

    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppUsbStorage::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
}

void AppUsbStorage::render()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(5, 5);
    GetHAL().canvas.println("USB Storage Mode");

    auto sd_info = GetHAL().sdCardProbe();

    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(5, 25);

    if (!sd_info.is_mounted) {
        GetHAL().canvas.setTextColor(TFT_RED, THEME_COLOR_BG);
        GetHAL().canvas.println("SD Card not found!");
        GetHAL().canvas.setCursor(5, 40);
        GetHAL().canvas.println("Insert SD card");
    } else {
        GetHAL().canvas.printf("SD: %s", sd_info.size.c_str());
        GetHAL().canvas.setCursor(5, 40);
        GetHAL().canvas.printf("Type: %s", sd_info.type.c_str());

        GetHAL().canvas.setCursor(5, 60);
        GetHAL().canvas.println("Status:");
        GetHAL().canvas.setCursor(5, 75);

        if (_msc_active) {
            GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
            GetHAL().canvas.println("[ ACTIVE ]");
            GetHAL().canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
            GetHAL().canvas.setCursor(5, 90);
            GetHAL().canvas.println("SD card exposed to PC");
            GetHAL().canvas.setTextColor(TFT_DARKGREY, THEME_COLOR_BG);
            GetHAL().canvas.setCursor(5, 105);
            GetHAL().canvas.println("Press ENTER to disable");
        } else {
            GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
            GetHAL().canvas.println("[ INACTIVE ]");
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
            GetHAL().canvas.setCursor(5, 90);
            GetHAL().canvas.println("Press ENTER to enable");
            GetHAL().canvas.setTextColor(TFT_DARKGREY, THEME_COLOR_BG);
            GetHAL().canvas.setCursor(5, 105);
            GetHAL().canvas.println("USB MSC mode");
        }
    }

    GetHAL().pushCanvas();
}

void AppUsbStorage::toggle_msc()
{
    auto sd_info = GetHAL().sdCardProbe();
    if (!sd_info.is_mounted) {
        mclog::tagWarn(getAppInfo().name, "SD card not mounted");
        return;
    }

    if (usb_msc_helper_toggle()) {
        _msc_active = usb_msc_helper_is_active();
        mclog::tagInfo(getAppInfo().name, "MSC state: {}", _msc_active ? "active" : "inactive");
    } else {
        mclog::tagError(getAppInfo().name, "Failed to toggle MSC");
    }

    render();
}
