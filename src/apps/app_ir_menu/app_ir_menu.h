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

class AppIrMenu : public mooncake::AppAbility {
public:
    AppIrMenu();
    ~AppIrMenu();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    class CategorySelectorMenu;
    CategorySelectorMenu* _menu = nullptr;

    int _remote_app_id    = -1;
    AppIcon_t* _remote_app_icon = nullptr;
    int _irfiles_app_id   = -1;
    AppIcon_t* _irfiles_app_icon = nullptr;
    int _stringir_app_id  = -1;
    AppIcon_t* _stringir_app_icon = nullptr;
    int _child_app_id     = -1;
    bool _is_suspended    = false;

    void resolve_app_ids();
};
