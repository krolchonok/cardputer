/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <hal/hal.h>

class AppSettings : public mooncake::AppAbility {
public:
    AppSettings();
    ~AppSettings();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    static constexpr int k_item_count = 2;
    static constexpr int k_volume_step = 1;

    int _key_event_slot_id = -1;
    int _selected_index    = 0;
    bool _sfx_enabled      = true;
    uint8_t _sfx_volume    = 10;

    void handle_key_event(const Keyboard::KeyEventRaw_t& keyEvent);
    void render_interface();
    void toggle_sfx();
    void adjust_volume(int delta);
};
