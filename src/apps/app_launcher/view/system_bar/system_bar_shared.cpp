#include "system_bar_shared.h"
#include <hal/hal.h>
#include <apps/utils/theme.h>
#include <apps/utils/common.h>
#include <mooncake_log.h>
#include <ctime>

#include "assets/bat1.h"
#include "assets/bat2.h"
#include "assets/bat3.h"
#include "assets/bat4.h"
#include "assets/wifi1.h"
#include "assets/wifi2.h"
#include "assets/wifi3.h"
#include "assets/wifi4.h"
#include "assets/wifi5.h"


void render_system_bar_shared()
{
    // Compute state
    int wifi_state = GetHAL().isWifiConnected() ? 1 : 4;

    // Time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    auto time_str = fmt::format("{:02d}:{:02d}", timeinfo.tm_hour, timeinfo.tm_min);

    // Battery
    auto bat_level = GetHAL().getBatLevel();
    int bat_state = 4;
    if (bat_level >= 100) {
        bat_state = 1;
    } else if (bat_level >= 75) {
        bat_state = 2;
    } else if (bat_level >= 50) {
        bat_state = 3;
    } else {
        bat_state = 4;
    }

    // Draw
    int margin_x = 5;
    int margin_y = 4;

    GetHAL().canvasSystemBar.fillScreen(THEME_COLOR_BG);
    GetHAL().canvasSystemBar.fillSmoothRoundRect(margin_x, margin_y, GetHAL().canvasSystemBar.width() - margin_x * 2,
                                                 GetHAL().canvasSystemBar.height() - margin_y * 2,
                                                 (GetHAL().canvasSystemBar.height() - margin_y * 2) / 2,
                                                 THEME_COLOR_SYSTEM_BAR);

    GetHAL().canvasSystemBar.setFont(FONT_BASIC);

    // Time (center)
    GetHAL().canvasSystemBar.setTextColor(THEME_COLOR_SYSTEM_BAR_TEXT);
    GetHAL().canvasSystemBar.drawCenterString(time_str.c_str(), GetHAL().canvasSystemBar.width() / 2,
                                              GetHAL().canvasSystemBar.height() / 2 - FONT_HEIGHT / 2);

    // Wifi icons (left)
    int x = 15;
    int y = 5;

    if (wifi_state == 1) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi1);
    } else if (wifi_state == 2) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi2);
    } else if (wifi_state == 3) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi3);
    } else if (wifi_state == 4) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi4);
    } else if (wifi_state == 5) {
        GetHAL().canvasSystemBar.pushImage(x, y, 16, 16, image_data_wifi5);
    }

    // Bat icon (right)
    x = GetHAL().canvasSystemBar.width() - 45;
    y = 5;

    if (bat_state == 1) {
        GetHAL().canvasSystemBar.pushImage(x, y, 32, 16, image_data_bat1);
    } else if (bat_state == 2) {
        GetHAL().canvasSystemBar.pushImage(x, y, 32, 16, image_data_bat2);
    } else if (bat_state == 3) {
        GetHAL().canvasSystemBar.pushImage(x, y, 32, 16, image_data_bat3);
    } else if (bat_state == 4) {
        GetHAL().canvasSystemBar.pushImage(x, y, 32, 16, image_data_bat4);
    }

    // Bat level number
    GetHAL().canvasSystemBar.setFont(&fonts::Font0);
    GetHAL().canvasSystemBar.setTextColor((uint32_t)0x000000);
    GetHAL().canvasSystemBar.drawCenterString(fmt::format("{}", bat_level).c_str(), 176,
                                              GetHAL().canvasSystemBar.height() / 2 - 3);

    GetHAL().pushCanvasSystemBar();
}
