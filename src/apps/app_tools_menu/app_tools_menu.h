/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <hal/hal.h>

struct AppIcon_t;

class AppToolsMenu : public mooncake::AppAbility {
public:
    AppToolsMenu();
    ~AppToolsMenu();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    class CategorySelectorMenu;
    CategorySelectorMenu* _menu = nullptr;

    int _clock_app_id   = -1;
    AppIcon_t* _clock_app_icon = nullptr;
    int _imu_app_id     = -1;
    AppIcon_t* _imu_app_icon = nullptr;
    int _sdcard_app_id  = -1;
    AppIcon_t* _sdcard_app_icon = nullptr;
    int _record_app_id  = -1;
    AppIcon_t* _record_app_icon = nullptr;
    int _chat_app_id    = -1;
    AppIcon_t* _chat_app_icon = nullptr;
    int _keyboard_app_id = -1;
    AppIcon_t* _keyboard_app_icon = nullptr;
    int _explorer_app_id = -1;
    AppIcon_t* _explorer_app_icon = nullptr;
    int _usb_app_id = -1;
    AppIcon_t* _usb_app_icon = nullptr;
    int _webserver_app_id = -1;
    AppIcon_t* _webserver_app_icon = nullptr;
    int _child_app_id   = -1;
    bool _is_suspended  = false;

    void resolve_app_ids();

};
