/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <hal/hal.h>

class AppDisplay : public mooncake::AppAbility {
public:
    AppDisplay();
    ~AppDisplay();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    static constexpr int k_step = 1;
    static constexpr int k_item_count = 3;
    static constexpr const char* k_setting_brightness = "disp_bright";
    static constexpr const char* k_setting_indicators = "disp_kb_ind";
    static constexpr const char* k_setting_font = "disp_font";

    int _key_event_slot_id = -1;
    int _selected_index    = 0;
    uint8_t _brightness    = 10;
    bool _indicators_enabled = true;
    int _font_index = 0;

    void handle_key_event(const Keyboard::KeyEventRaw_t& keyEvent);
    void render_interface();
    void adjust_brightness(int delta);
    void toggle_indicators();
    void cycle_font();
};
