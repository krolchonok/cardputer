/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_settings_menu.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <smooth_ui_toolkit.h>
#include "app_set_wifi/assets/set_wifi_big.h"
#include "app_set_wifi/assets/set_wifi_small.h"

using namespace mooncake;
using namespace smooth_ui_toolkit;

class AppSettingsMenu::SettingsSelectorMenu : public SmoothSelectorMenu {
public:
    struct OptionInfo_t {
        int appId = -1;
        std::string name;
    };

    std::function<void(int appId)> onOpen;

    void init(const std::vector<OptionInfo_t>& options)
    {
        _options = options;

        if (options.empty()) {
            return;
        }

        const int total_width = static_cast<int>(options.size()) * ICON_WIDTH +
                                static_cast<int>(options.size() - 1) * ICON_GAP;
        const int start_x = (GetHAL().canvas.width() - total_width) / 2;

        for (size_t i = 0; i < options.size(); ++i) {
            int x = start_x + static_cast<int>(i) * (ICON_WIDTH + ICON_GAP);
            addOption({Vector4i{x, ICON_MARGIN_TOP, ICON_WIDTH, ICON_WIDTH}, nullptr});
        }

        getSelectorPostion().x.springOptions().visualDuration = 0.4;
        getSelectorShape().x.springOptions().visualDuration   = 0.4;
        getSelectorShape().y.springOptions().visualDuration   = 0.4;

        moveTo(0);
        setConfig().readInputInterval = 0;
    }

    void onReadInput() override
    {
        auto raw = GetHAL().keyboard.getLatestKeyEventRaw();
        if (raw.state) {
            if (raw.row == 3 && raw.col == 10) {
                goLast();
                return;
            }
            if (raw.row == 3 && raw.col == 12) {
                goNext();
                return;
            }
            if (raw.row == 2 && raw.col == 11) {
                goLast();
                return;
            }
            if (raw.row == 3 && raw.col == 11) {
                goNext();
                return;
            }
            if (raw.row == 2 && raw.col == 13) {
                press(getSelectedKeyframe());
                return;
            }
        } else {
            if (raw.row == 2 && raw.col == 13) {
                release();
                return;
            }
        }

        auto key = GetHAL().keyboard.getLatestKeyEvent();
        if (!key.state) {
            return;
        }
        switch (key.keyCode) {
            case KEY_LEFT:
            case KEY_UP:
                goLast();
                break;
            case KEY_RIGHT:
            case KEY_DOWN:
                goNext();
                break;
            case KEY_ENTER:
                press(getSelectedKeyframe());
                break;
            default:
                break;
        }
    }

    void onClick() override
    {
        if (onOpen && getSelectedOptionIndex() >= 0 &&
            getSelectedOptionIndex() < static_cast<int>(_options.size())) {
            onOpen(_options[getSelectedOptionIndex()].appId);
        }
    }

    void onRender() override
    {
        auto selector_kf = getSelectorCurrentFrame();
        GetHAL().canvas.fillScreen(THEME_COLOR_BG);

        int x_offset = -(selector_kf.x) + GetHAL().canvas.width() / 2 - ICON_WIDTH / 2;

        GetHAL().canvas.setFont(FONT_BASIC);
        GetHAL().canvas.setTextSize(1);
        GetHAL().canvas.setTextColor(THEME_COLOR_ICON, THEME_COLOR_BG);

        int index = 0;
        for (auto& opt : getOptionList()) {
            bool selected = index == getSelectedOptionIndex();
            int box_size  = selected ? ICON_SELECTED_WIDTH : ICON_WIDTH;
            int box_x     = opt.keyframe.x - (box_size - opt.keyframe.width) / 2 + x_offset;
            int box_y     = opt.keyframe.y - (box_size - opt.keyframe.height) / 2;

            GetHAL().canvas.fillSmoothRoundRect(box_x, box_y, box_size, box_size, 8, THEME_COLOR_ICON);

            const char* label = _options[index].name.c_str();
            GetHAL().canvas.setTextColor(TFT_BLACK, THEME_COLOR_ICON);
            GetHAL().canvas.drawCenterString(label, box_x + box_size / 2, box_y + box_size / 2 - 6);

            GetHAL().canvas.setTextColor(THEME_COLOR_ICON, THEME_COLOR_BG);
            GetHAL().canvas.drawCenterString(
                label, opt.keyframe.x + opt.keyframe.width / 2 + x_offset,
                opt.keyframe.y + opt.keyframe.height + ICON_TAG_MARGIN_TOP + (box_size - opt.keyframe.width) / 2);

            index++;
        }

        GetHAL().pushCanvas();
    }

private:
    std::vector<OptionInfo_t> _options;
};

AppSettingsMenu::AppSettingsMenu()
{
    setAppInfo().name = "Settings";
    setAppInfo().userData = new AppIcon_t(image_data_set_wifi_big, image_data_set_wifi_small);
}

AppSettingsMenu::~AppSettingsMenu()
{
    delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppSettingsMenu::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    resolve_app_ids();

    _menu = new SettingsSelectorMenu();
    std::vector<SettingsSelectorMenu::OptionInfo_t> options;
    if (_wifi_app_id >= 0) {
        options.push_back({_wifi_app_id, "WiFi"});
    }
    if (_display_app_id >= 0) {
        options.push_back({_display_app_id, "Display"});
    }
    if (_sound_app_id >= 0) {
        options.push_back({_sound_app_id, "Sound"});
    }
    _menu->init(options);
    _menu->onOpen = [this](int appId) {
        if (appId >= 0) {
            _child_app_id = appId;
            _is_suspended = true;
            GetMooncake().openApp(appId);
        }
    };
}

void AppSettingsMenu::onRunning()
{
    if (_is_suspended) {
        if (_child_app_id >= 0 &&
            GetMooncake().getAppCurrentState(_child_app_id) == AppAbility::StateSleeping) {
            _child_app_id = -1;
            _is_suspended = false;
            if (_menu) {
                _menu->update();
            }
        }
        return;
    }

    if (_menu) {
        _menu->update();
    }

    // Display WiFi info at the top of screen
    GetHAL().canvasSystemBar.fillScreen(THEME_COLOR_BG);
    GetHAL().canvasSystemBar.setTextSize(1);
    GetHAL().canvasSystemBar.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvasSystemBar.setCursor(0, 0);
    
    if (GetHAL().isWifiConnected()) {
        GetHAL().canvasSystemBar.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvasSystemBar.printf("WiFi: %s", GetHAL().getWifiIpAddress().c_str());
    } else {
        GetHAL().canvasSystemBar.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
        GetHAL().canvasSystemBar.printf("WiFi: Disconnected");
    }
    
    GetHAL().pushCanvasSystemBar();

    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppSettingsMenu::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_menu) {
        delete _menu;
        _menu = nullptr;
    }

    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().pushCanvas();
}

void AppSettingsMenu::resolve_app_ids()
{
    _wifi_app_id    = -1;
    _sound_app_id   = -1;
    _display_app_id = -1;

    auto installed_apps = GetMooncake().getAppAbilityManager()->getAllAbilityInstance();
    for (auto& app_raw : installed_apps) {
        auto app = static_cast<AppAbility*>(app_raw);
        if (app->getAppInfo().name == "SetWiFi") {
            _wifi_app_id = app->getId();
        } else if (app->getAppInfo().name == "Sound") {
            _sound_app_id = app->getId();
        } else if (app->getAppInfo().name == "Display") {
            _display_app_id = app->getId();
        }
    }
}
