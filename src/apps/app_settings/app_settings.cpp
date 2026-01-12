/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_settings.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <algorithm>

using namespace mooncake;

AppSettings::AppSettings()
{
    setAppInfo().name = "Settings";
}

AppSettings::~AppSettings()
{
}

void AppSettings::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _selected_index = 0;
    _sfx_enabled    = audio::get_keyboard_sfx_enabled();
    _sfx_volume     = static_cast<uint8_t>(
        std::clamp<int>((audio::get_keyboard_sfx_volume() * 10 + 127) / 255, 0, 10));

    _key_event_slot_id = GetHAL().keyboard.onKeyEventRaw.connect(
        [this](const Keyboard::KeyEventRaw_t& keyEvent) { handle_key_event(keyEvent); });

    render_interface();
}

void AppSettings::onRunning()
{
    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppSettings::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_key_event_slot_id >= 0) {
        GetHAL().keyboard.onKeyEventRaw.disconnect(_key_event_slot_id);
        _key_event_slot_id = -1;
    }
}

void AppSettings::handle_key_event(const Keyboard::KeyEventRaw_t& keyEvent)
{
    if (!keyEvent.state) {
        return;
    }

    if (keyEvent.row == 2 && keyEvent.col == 11) {
        _selected_index = (_selected_index + k_item_count - 1) % k_item_count;
        render_interface();
        return;
    }
    if (keyEvent.row == 3 && keyEvent.col == 11) {
        _selected_index = (_selected_index + 1) % k_item_count;
        render_interface();
        return;
    }
    if (keyEvent.row == 3 && keyEvent.col == 10) {
        if (_selected_index == 0) {
            toggle_sfx();
        } else {
            adjust_volume(-k_volume_step);
        }
        return;
    }
    if (keyEvent.row == 3 && keyEvent.col == 12) {
        if (_selected_index == 0) {
            toggle_sfx();
        } else {
            adjust_volume(k_volume_step);
        }
        return;
    }
    if (keyEvent.row == 2 && keyEvent.col == 13) {
        if (_selected_index == 0) {
            toggle_sfx();
        }
        return;
    }
}

void AppSettings::toggle_sfx()
{
    _sfx_enabled = !_sfx_enabled;
    audio::set_keyboard_sfx_enable(_sfx_enabled);
    render_interface();
    if (_sfx_enabled) {
        audio::play_random_tone();
    }
}

void AppSettings::adjust_volume(int delta)
{
    int next = std::clamp<int>(_sfx_volume + delta, 0, 10);
    if (next == _sfx_volume) {
        return;
    }

    _sfx_volume = static_cast<uint8_t>(next);
    audio::set_keyboard_sfx_volume(static_cast<uint8_t>((_sfx_volume * 255 + 5) / 10));
    render_interface();
    if (_sfx_enabled) {
        audio::play_random_tone();
    }
}

void AppSettings::render_interface()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setCursor(0, 0);
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.println("Settings");

    const int row_h   = FONT_HEIGHT + 2;
    const int start_y = 18;

    for (int i = 0; i < k_item_count; ++i) {
        int y = start_y + i * row_h;

        if (i == _selected_index) {
            GetHAL().canvas.fillRect(0, y - 1, GetHAL().canvas.width(), FONT_HEIGHT + 2, THEME_COLOR_ICON);
            GetHAL().canvas.setTextColor(TFT_BLACK, THEME_COLOR_ICON);
        } else {
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
        }

        GetHAL().canvas.setCursor(2, y);
        if (i == 0) {
            GetHAL().canvas.printf("Key click sound: %s", _sfx_enabled ? "On" : "Off");
        } else if (i == 1) {
            GetHAL().canvas.printf("Key click volume: %u", _sfx_volume);
        }
    }

    const int hint_y = start_y + k_item_count * row_h + 6;
    GetHAL().canvas.setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(0, hint_y);
    GetHAL().canvas.print("Up/Down: select");
    GetHAL().canvas.setCursor(0, hint_y + FONT_HEIGHT);
    GetHAL().canvas.print("Left/Right: change");

    GetHAL().pushCanvas();
}
