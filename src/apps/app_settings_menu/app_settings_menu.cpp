/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_settings_menu.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <smooth_ui_toolkit.h>

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
        auto event = GetHAL().keyboard.getLatestKeyEventRaw();
        if (event.state) {
            if (event.row == 3 && event.col == 10) {
                goLast();
            } else if (event.row == 3 && event.col == 12) {
                goNext();
            } else if (event.row == 2 && event.col == 11) {
                goLast();
            } else if (event.row == 3 && event.col == 11) {
                goNext();
            } else if (event.row == 2 && event.col == 13) {
                press(getSelectedKeyframe());
            }
        } else {
            if (event.row == 2 && event.col == 13) {
                release();
            }
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
}

AppSettingsMenu::~AppSettingsMenu()
{
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
    _wifi_app_id  = -1;
    _sound_app_id = -1;

    auto installed_apps = GetMooncake().getAppAbilityManager()->getAllAbilityInstance();
    for (auto& app_raw : installed_apps) {
        auto app = static_cast<AppAbility*>(app_raw);
        if (app->getAppInfo().name == "SetWiFi") {
            _wifi_app_id = app->getId();
        } else if (app->getAppInfo().name == "Sound") {
            _sound_app_id = app->getId();
        }
    }
}
