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

    _selected_index = 0;
    int32_t saved = GetHAL().getSettings().GetInt(k_setting_brightness, 10);
    _brightness   = static_cast<uint8_t>(std::clamp<int32_t>(saved, 0, 10));
    _indicators_enabled = GetHAL().getSettings().GetInt(k_setting_indicators, 1) != 0;
    _font_index = GetHAL().getSettings().GetInt(k_setting_font, 0);
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
            toggle_indicators();
        } else if (_selected_index == 2) {
            cycle_font();
        } else {
            adjust_brightness(-k_step);
        }
        return;
    }
    if (keyEvent.row == 3 && keyEvent.col == 12) {
        if (_selected_index == 0) {
            toggle_indicators();
        } else if (_selected_index == 2) {
            cycle_font();
        } else {
            adjust_brightness(k_step);
        }
        return;
    }
    if (keyEvent.row == 2 && keyEvent.col == 13) {
        if (_selected_index == 0) {
            toggle_indicators();
        } else if (_selected_index == 2) {
            cycle_font();
        }
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

void AppDisplay::toggle_indicators()
{
    _indicators_enabled = !_indicators_enabled;
    GetHAL().getSettings().SetInt(k_setting_indicators, _indicators_enabled ? 1 : 0);
    GetHAL().setKeyboardBarVisible(_indicators_enabled);
    if (!_indicators_enabled) {
        GetHAL().canvasKeyboardBar.fillScreen(THEME_COLOR_BG);
        GetHAL().pushCanvasKeyboardBar();
    }
    render_interface();
}

void AppDisplay::cycle_font()
{
    _font_index = (_font_index + 1) % 4;
    GetHAL().getSettings().SetInt(k_setting_font, _font_index);
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
            GetHAL().canvas.printf("Indicators: %s", _indicators_enabled ? "On" : "Off");
        } else if (i == 1) {
            GetHAL().canvas.printf("Brightness: %u", _brightness);
        } else if (i == 2) {
            const char* font_names[] = {"Font0", "Font2", "Font4", "efontCN"};
            GetHAL().canvas.printf("Font: %s", font_names[_font_index]);
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
