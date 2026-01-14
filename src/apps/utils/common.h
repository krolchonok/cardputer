/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <hal/hal.h>

#define FW_VERSION "V0.3"

#define ANIM_APP_OPEN()

#define ANIM_APP_CLOSE()

struct AppIcon_t {
public:
    AppIcon_t(const uint16_t* iconBig, const uint16_t* iconSmall)
    {
        this->iconBig   = iconBig;
        this->iconSmall = iconSmall;
    }

    const uint16_t* iconBig;
    const uint16_t* iconSmall;
};
