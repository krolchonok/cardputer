/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_display.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <algorithm>

using namespace mooncake;

AppDisplay::AppDisplay()
{
    setAppInfo().name = "Display";
}

AppDisplay::~AppDisplay()
{
}

void AppDisplay::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    int32_t saved = GetHAL().getSettings().GetInt(k_setting_brightness, 10);
    _brightness   = static_cast<uint8_t>(std::clamp<int32_t>(saved, 0, 10));
    uint8_t hw_brightness = static_cast<uint8_t>((_brightness * 255 + 5) / 10);
    GetHAL().display.setBrightness(hw_brightness);
    _key_event_slot_id = GetHAL().keyboard.onKeyEventRaw.connect(
        [this](const Keyboard::KeyEventRaw_t& keyEvent) { handle_key_event(keyEvent); });

    render_interface();
}

void AppDisplay::onRunning()
{
    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppDisplay::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_key_event_slot_id >= 0) {
        GetHAL().keyboard.onKeyEventRaw.disconnect(_key_event_slot_id);
        _key_event_slot_id = -1;
    }

    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().pushCanvas();
}

void AppDisplay::handle_key_event(const Keyboard::KeyEventRaw_t& keyEvent)
{
    if (!keyEvent.state) {
        return;
    }

    if (keyEvent.row == 3 && keyEvent.col == 10) {
        adjust_brightness(-k_volume_step);
        return;
    }
    if (keyEvent.row == 3 && keyEvent.col == 12) {
        adjust_brightness(k_volume_step);
        return;
    }
    if (keyEvent.row == 2 && keyEvent.col == 13) {
        return;
    }
}

void AppDisplay::adjust_brightness(int delta)
{
    int next = std::clamp<int>(_brightness + delta, 0, 10);
    if (next == _brightness) {
        return;
    }

    _brightness = static_cast<uint8_t>(next);
    uint8_t hw_brightness = static_cast<uint8_t>((_brightness * 255 + 5) / 10);
    GetHAL().display.setBrightness(hw_brightness);
    GetHAL().getSettings().SetInt(k_setting_brightness, _brightness);
    render_interface();
}

void AppDisplay::render_interface()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.println("Display");

    const int row_h   = FONT_HEIGHT + 2;
    const int start_y = 18;
    int y = start_y;

    GetHAL().canvas.fillRect(0, y - 1, GetHAL().canvas.width(), FONT_HEIGHT + 2, THEME_COLOR_ICON);
    GetHAL().canvas.setTextColor(TFT_BLACK, THEME_COLOR_ICON);
    GetHAL().canvas.setCursor(2, y);
    GetHAL().canvas.printf("Brightness: %u", _brightness);

    const int hint_y = start_y + row_h + 6;
    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, hint_y);
    GetHAL().canvas.print("Left/Right: change");

    GetHAL().pushCanvas();
}
