/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../../app_launcher.h"
#include <mooncake_log.h>
#include <apps/utils/theme.h>
#include <apps/utils/common.h>

#include "assets/bat1.h"
#include "assets/bat2.h"
#include "assets/bat3.h"
#include "assets/bat4.h"
#include "assets/wifi1.h"
#include "assets/wifi2.h"
#include "assets/wifi3.h"
#include "assets/wifi4.h"
#include "assets/wifi5.h"

void Launcher::start_system_bar()
{
    render_system_bar();
}

void Launcher::update_system_bar()
{
    if ((GetHAL().millis() - _data.system_bar_update_count) > _data.system_bar_update_preiod) {
        render_system_bar();
        _data.system_bar_update_count = GetHAL().millis();
    }
}

#include "system_bar_shared.h"

void Launcher::render_system_bar()
{
    // Use shared renderer to ensure consistent behavior across apps
    render_system_bar_shared();
}
