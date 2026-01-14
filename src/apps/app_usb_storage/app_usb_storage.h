/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <hal/hal.h>

struct AppIcon_t;

class AppUsbStorage : public mooncake::AppAbility {
public:
    AppUsbStorage();
    ~AppUsbStorage();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    bool _msc_active = false;
    unsigned long _last_update = 0;

    void render();
    void toggle_msc();
};
