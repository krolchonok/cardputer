/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "keyboard/keyboard.h"
#include "cap_lora868/cap_lora868.h"
#include "utils/settings/settings.h"
#include <M5Unified.hpp>
#include <M5GFX.h>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

class Hal {
public:
    void init();
    void update();

    /* --------------------------------- System --------------------------------- */
    void delay(std::uint32_t ms)
    {
        m5gfx::delay(ms);
    }
    std::uint32_t millis()
    {
        return m5gfx::millis();
    }
    void feedTheDog();
    std::vector<uint8_t> getDeviceMac();
    std::string getDeviceMacString();

    /* --------------------------------- Display -------------------------------- */
    M5GFX& display                = M5.Display;
    LGFX_Sprite canvas            = LGFX_Sprite(&M5.Display);
    LGFX_Sprite canvasSystemBar   = LGFX_Sprite(&M5.Display);
    LGFX_Sprite canvasKeyboardBar = LGFX_Sprite(&M5.Display);

    inline void pushCanvasSystemBar()
    {
        int x = _keyboard_bar_visible ? _keyboard_bar_width : 0;
        canvasSystemBar.pushSprite(x, 0);
    }
    inline void pushCanvasKeyboardBar()
    {
        if (!_keyboard_bar_visible) {
            return;
        }
        canvasKeyboardBar.pushSprite(0, 0);
    }
    inline void pushCanvas()
    {
        int x = _keyboard_bar_visible ? _keyboard_bar_width : 0;
        canvas.pushSprite(x, _system_bar_height);
    }

    /* ---------------------------------- Audio --------------------------------- */
    m5::Speaker_Class& speaker = M5.Speaker;
    m5::Mic_Class& mic         = M5.Mic;

    /* ---------------------------------- Input --------------------------------- */
    m5::Button_Class& homeButton = M5.BtnA;
    Keyboard keyboard;

    /* ---------------------------------- Power --------------------------------- */
    inline uint8_t getBatLevel()
    {
        return M5.Power.getBatteryLevel();
    }

    /* ---------------------------------- WiFi ---------------------------------- */
    using ScanResult_t = std::pair<int, std::string>;
    void wifiInit();
    void wifiDeinit();
    void wifiScan(std::vector<ScanResult_t>& scanResult);
    bool wifiConnect(const std::string& ssid, const std::string& password);
    bool isWifiConnected() const
    {
        return _is_wifi_connected;
    }
    void wifiDisconnect();

    /* --------------------------------- EspNow --------------------------------- */
    void espNowInit();
    void espNowDeinit();
    void espNowSend(const std::string& data);
    bool espNowAvailable();
    const std::string& espNowGetReceivedData();
    void espNowClearReceivedData();

    /* ----------------------------------- IR ----------------------------------- */
    void irInit();
    void irSend(uint8_t addr, uint8_t cmd);

    /* ----------------------------------- BLE ---------------------------------- */
    void bleKeyboardInit();
    bool bleKeyboardIsConnected() const;
    const char* getBleKeyboardName() const;

    /* ----------------------------------- USB ---------------------------------- */
    void usbKeyboardInit();
    bool usbKeyboardIsConnected() const;
    void usbKeyboardDeinit();
    void usbSwitchToSerialJtag();

    /* -------------------------------- Settings -------------------------------- */
    Settings& getSettings()
    {
        return *_settings;
    }
    void setKeyboardBarVisible(bool visible);
    bool isKeyboardBarVisible() const
    {
        return _keyboard_bar_visible;
    }

    /* ----------------------------------- IMU ---------------------------------- */
    m5::IMU_Class& imu = M5.Imu;

    /* --------------------------------- SD Card -------------------------------- */
    struct SdCardProbeResult_t {
        bool is_mounted = false;
        std::string size;
        std::string type;
        std::string name;

        bool operator==(const SdCardProbeResult_t& other) const
        {
            return is_mounted == other.is_mounted && size == other.size && type == other.type && name == other.name;
        }
    };

    SdCardProbeResult_t sdCardProbe();

    /* ----------------------------------- Cap ---------------------------------- */
    CapLoRa868 capLora868;

private:
    static constexpr int k_canvas_width_with_bar = 204;
    static constexpr int k_canvas_height = 109;

    Settings* _settings             = nullptr;
    bool _is_wifi_inited            = false;
    bool _is_wifi_connected         = false;
    bool _is_esp_now_inited         = false;
    bool _is_ir_inited              = false;
    bool _is_ble_keyboard_inited    = false;
    bool _is_usb_keyboard_inited    = false;
    bool _is_sd_card_mounted        = false;
    int _ble_keyboard_event_slot_id = -1;
    int _usb_keyboard_event_slot_id = -1;
    bool _keyboard_bar_visible      = true;
    int _keyboard_bar_width         = 0;
    int _system_bar_height          = 0;

    std::unique_ptr<CapLoRa868> _cap_lora868;

    void display_init();
    void applyDisplayLayout();
    void i2c_scan();
    void keyboard_init();
    void start_sntp();
    void stop_sntp();
    void setting_init();
    void spi_init();
    void sd_card_init();
    void handle_ble_keyboard_event(const Keyboard::KeyEvent_t& keyEvent);
    void handle_usb_keyboard_event(const Keyboard::KeyEvent_t& keyEvent);
};

Hal& GetHAL();
