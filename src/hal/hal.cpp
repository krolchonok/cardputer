/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "hal_config.h"
#include <Arduino.h>
#include <apps/utils/audio/audio.h>
#include <mooncake_log.h>
#include <M5Unified.hpp>
#include <memory>
#include <algorithm>
#include <time.h>
#if !defined(ARDUINO)
#include <esp_mac.h>
#endif
#if defined(ARDUINO)
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <hal/utils/usb_msc/usb_msc_helper.h>
#endif

static std::unique_ptr<Hal> _hal_instance;
static const std::string _tag = "HAL";

Hal& GetHAL()
{
    if (!_hal_instance) {
        mclog::tagInfo(_tag, "creating hal instance");
        _hal_instance = std::make_unique<Hal>();
    }
    return *_hal_instance.get();
}

void Hal::init()
{
    mclog::tagInfo(_tag, "init");

    M5.begin();
    M5.Display.setBrightness(0);
    M5.Speaker.begin();  // Codec takes some time to initialize

    display_init();
    i2c_scan();
    keyboard_init();
    setting_init();
    spi_init();
    
    // Initialize USB MSC (for SD card Mass Storage Class support)
    usb_msc_helper_init();
    
    // Attempt auto-connect to saved WiFi
    wifiAutoConnect();
}

void Hal::update()
{
    M5.update();
    keyboard.update();
    capLora868.update();
    
    // Check background WiFi connection status
    if (_is_wifi_inited && !_is_wifi_connected && WiFi.status() == WL_CONNECTED) {
        _is_wifi_connected = true;
        start_sntp();
        mclog::tagInfo(_tag, "WiFi connected in background");
    }
}

void Hal::feedTheDog()
{
#if defined(ARDUINO)
    delay(1);
#else
    vTaskDelay(1);
#endif
}

std::vector<uint8_t> Hal::getDeviceMac()
{
    std::vector<uint8_t> mac(6);
#if defined(ARDUINO)
    uint64_t mac64 = ESP.getEfuseMac();
    for (int i = 0; i < 6; i++) {
        mac[5 - i] = (mac64 >> (8 * i)) & 0xFF;
    }
#else
    esp_read_mac(mac.data(), ESP_MAC_EFUSE_FACTORY);
#endif
    return mac;
}

std::string Hal::getDeviceMacString()
{
    auto mac = getDeviceMac();
    return fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

std::string Hal::getBleMacString()
{
    // BLE MAC is base MAC + 2
    auto mac = getDeviceMac();
    // Add 2 to the MAC address (BLE uses base MAC + 2)
    uint64_t macVal = 0;
    for (int i = 0; i < 6; i++) {
        macVal = (macVal << 8) | mac[i];
    }
    macVal += 2;
    uint8_t bleMac[6];
    for (int i = 5; i >= 0; i--) {
        bleMac[i] = macVal & 0xFF;
        macVal >>= 8;
    }
    return fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", bleMac[0], bleMac[1], bleMac[2], bleMac[3], bleMac[4], bleMac[5]);
}

/* -------------------------------------------------------------------------- */
/*                                  Dispplay                                  */
/* -------------------------------------------------------------------------- */
void Hal::display_init()
{
    mclog::tagInfo(_tag, "display init");

    _keyboard_bar_visible = true;
    applyDisplayLayout();
}

void Hal::applyDisplayLayout()
{
    int canvas_width = _keyboard_bar_visible ? k_canvas_width_with_bar : display.width();
    int canvas_height = k_canvas_height;
    int kb_width = display.width() - canvas_width;
    if (!_keyboard_bar_visible) {
        kb_width = 0;
    }
    if (kb_width < 0) {
        kb_width = 0;
    }

    _keyboard_bar_width = kb_width;
    _system_bar_height  = display.height() - canvas_height;

    canvas.deleteSprite();
    canvasKeyboardBar.deleteSprite();
    canvasSystemBar.deleteSprite();

    canvas.createSprite(canvas_width, canvas_height);
    if (kb_width > 0) {
        canvasKeyboardBar.createSprite(kb_width, display.height());
    } else {
        canvasKeyboardBar.createSprite(1, display.height());
    }
    canvasSystemBar.createSprite(canvas_width, _system_bar_height);
}

void Hal::setKeyboardBarVisible(bool visible)
{
    if (visible == _keyboard_bar_visible) {
        return;
    }

    _keyboard_bar_visible = visible;
    applyDisplayLayout();
}

/* -------------------------------------------------------------------------- */
/*                                     I2C                                    */
/* -------------------------------------------------------------------------- */
void Hal::i2c_scan()
{
    mclog::tagInfo(_tag, "i2c scan");

    bool ret[128] = {false};
    M5.In_I2C.scanID(ret);

    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            if (ret[address]) {
                printf("%02x ", address);
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }
}

/* -------------------------------------------------------------------------- */
/*                                  Settings                                  */
/* -------------------------------------------------------------------------- */
// https://github.com/78/xiaozhi-esp32/blob/main/main/main.cc

void Hal::setting_init()
{
    mclog::tagInfo(_tag, "setting init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        mclog::tagWarn(_tag, "erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    _settings = new Settings("cardputer", true);
}

/* -------------------------------------------------------------------------- */
/*                                  Keyboard                                  */
/* -------------------------------------------------------------------------- */
void Hal::keyboard_init()
{
    mclog::tagInfo(_tag, "keyboard init");

    if (!keyboard.init()) {
        mclog::tagError(_tag, "keyboard init failed");
        return;
    }
}

/* -------------------------------------------------------------------------- */
/*                                    WiFI                                    */
/* -------------------------------------------------------------------------- */
#if defined(ARDUINO)
void Hal::wifiScan(std::vector<ScanResult_t>& scanResult)
{
    mclog::tagInfo(_tag, "wifi scan");

    scanResult.clear();
    int count = WiFi.scanNetworks();
    if (count <= 0) {
        mclog::tagWarn(_tag, "wifi scan completed, found 0 APs");
        return;
    }

    for (int i = 0; i < count; i++) {
        std::string ssid = WiFi.SSID(i).c_str();
        int rssi         = WiFi.RSSI(i);
        if (!ssid.empty()) {
            scanResult.push_back(std::make_pair(rssi, ssid));
        }
    }

    std::sort(scanResult.begin(), scanResult.end(),
              [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
                  return a.first > b.first;
              });

    mclog::tagInfo(_tag, "wifi scan completed, found {} APs", scanResult.size());
}

void Hal::wifiInit()
{
    mclog::tagInfo(_tag, "wifi init");

    if (_is_wifi_inited) {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    _is_wifi_inited = true;
}

void Hal::wifiDeinit()
{
    mclog::tagInfo(_tag, "wifi deinit");

    if (!_is_wifi_inited) {
        return;
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _is_wifi_inited = false;
    _is_wifi_connected = false;
}

bool Hal::wifiConnect(const std::string& ssid, const std::string& password)
{
    mclog::tagInfo(_tag, "wifi connect to ssid: {} password: {}", ssid, password);

    if (!_is_wifi_inited) {
        wifiInit();
    }

    WiFi.begin(ssid.c_str(), password.c_str());
    const uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < 10000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        _is_wifi_connected = true;
        start_sntp();
        return true;
    }

    mclog::tagError(_tag, "failed to connect to SSID: {}", ssid);
    return false;
}

void Hal::wifiDisconnect()
{
    mclog::tagInfo(_tag, "wifi disconnect");

    if (!_is_wifi_inited || !_is_wifi_connected) {
        return;
    }

    WiFi.disconnect();
    _is_wifi_connected = false;
    stop_sntp();
}

std::string Hal::getWifiIpAddress() const
{
    if (!_is_wifi_connected) {
        return "";
    }
    return WiFi.localIP().toString().c_str();
}

std::string Hal::getWifiSsid() const
{
    if (!_is_wifi_connected) {
        return "";
    }
    return WiFi.SSID().c_str();
}

void Hal::wifiAutoConnect()
{
    mclog::tagInfo(_tag, "wifi auto connect");

    // Check if we have saved WiFi credentials
    std::string saved_ssid = getSettings().GetString("wifi_ssid", "");
    std::string saved_password = getSettings().GetString("wifi_password", "");

    if (saved_ssid.empty() || saved_password.empty()) {
        mclog::tagWarn(_tag, "no saved WiFi credentials found");
        return;
    }

    mclog::tagInfo(_tag, "attempting auto connect to: {}", saved_ssid);
    
    // Non-blocking WiFi connection at startup for faster boot
    // WiFi will connect in background, apps can check wifiIsConnected() later
    if (!_is_wifi_inited) {
        wifiInit();
    }
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    mclog::tagInfo(_tag, "WiFi connecting in background to: {}", saved_ssid);
}

void Hal::start_sntp()
{
    mclog::tagInfo(_tag, "start sntp");

    if (!_is_wifi_connected) {
        mclog::tagError(_tag, "wifi not connected");
        return;
    }

    configTime(8 * 3600, 0, "pool.ntp.org");
}

void Hal::stop_sntp()
{
    mclog::tagInfo(_tag, "stop sntp");
}

#else
// https://github.com/espressif/esp-idf/blob/v5.3.3/examples/wifi/scan/main/scan.c
// https://github.com/espressif/esp-idf/blob/v5.4.2/examples/wifi/getting_started/station/main/station_example_main.c
// http://github.com/espressif/esp-idf/blob/v5.4.2/examples/protocols/sntp/main/sntp_example_main.c
#include <esp_wifi.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_err.h>
#include <esp_system.h>
#include <esp_event.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <vector>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

#define DEFAULT_SCAN_LIST_SIZE 6
static wifi_ap_record_t _ap_info[DEFAULT_SCAN_LIST_SIZE];

void Hal::wifiScan(std::vector<ScanResult_t>& scanResult)
{
    mclog::tagInfo(_tag, "wifi scan");

    scanResult.clear();

    uint16_t number   = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    memset(_ap_info, 0, sizeof(_ap_info));

    // Start WiFi scan
    esp_err_t ret = esp_wifi_scan_start(NULL, true);
    if (ret != ESP_OK) {
        mclog::tagError(_tag, "failed to start wifi scan: {}", esp_err_to_name(ret));
        return;
    }

    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        mclog::tagError(_tag, "failed to get AP number: {}", esp_err_to_name(ret));
        return;
    }

    ret = esp_wifi_scan_get_ap_records(&number, _ap_info);
    if (ret != ESP_OK) {
        mclog::tagError(_tag, "failed to get AP records: {}", esp_err_to_name(ret));
        return;
    }

    // Process scan results
    for (int i = 0; i < number; i++) {
        std::string ssid = (char*)_ap_info[i].ssid;
        int rssi         = _ap_info[i].rssi;

        // Skip empty SSID
        if (ssid.empty()) {
            continue;
        }

        // Add to ap_list
        scanResult.push_back(std::make_pair(rssi, ssid));
    }

    // Sort ap_list by RSSI (from strongest to weakest)
    std::sort(scanResult.begin(), scanResult.end(),
              [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
                  return a.first > b.first;  // Higher RSSI first
              });

    mclog::tagInfo(_tag, "wifi scan completed, found {} APs", scanResult.size());
}

static EventGroupHandle_t s_wifi_event_group = NULL;
static const int WIFI_CONNECTED_BIT          = BIT0;
static const int WIFI_DISCONNECTED_BIT       = BIT1;
static const int WIFI_FAIL_BIT               = BIT2;
static const int WIFI_STARTED_BIT            = BIT3;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    const char* TAG = "wifi";

    // Wifi started
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
    }

    // Disconnected
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }

    // Connected
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void Hal::wifiInit()
{
    mclog::tagInfo(_tag, "wifi init");

    if (_is_wifi_inited) {
        return;
    }

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

    ESP_ERROR_CHECK(esp_wifi_start());
    _is_wifi_inited = true;
}

void Hal::wifiDeinit()
{
    mclog::tagInfo(_tag, "wifi deinit");

    if (!_is_wifi_inited) {
        return;
    }

    esp_wifi_stop();
    esp_wifi_deinit();
    _is_wifi_inited = false;
}

bool Hal::wifiConnect(const std::string& ssid, const std::string& password)
{
    mclog::tagInfo(_tag, "wifi connect to ssid: {} password: {}", ssid, password);

    if (!_is_wifi_inited) {
        wifiInit();
    }

    wifiDisconnect();

    // Hold until wifi started
    xEventGroupWaitBits(s_wifi_event_group, WIFI_STARTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(3000));

    // Reset event status
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_DISCONNECTED_BIT);

    // Set Wi-Fi config
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait for connection result
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        mclog::tagInfo(_tag, "connected to SSID: {}", ssid);
        _is_wifi_connected = true;
        start_sntp();
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        mclog::tagError(_tag, "failed to connect to SSID: {}", ssid);
        return false;
    } else {
        mclog::tagError(_tag, "wifi connect timeout");
        return false;
    }
}

void Hal::wifiDisconnect()
{
    mclog::tagInfo(_tag, "wifi disconnect");

    if (!_is_wifi_inited) {
        return;
    }

    if (!_is_wifi_connected) {
        return;
    }

    // Hold until wifi started
    xEventGroupWaitBits(s_wifi_event_group, WIFI_STARTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(3000));

    // Disconnect old connection
    ESP_ERROR_CHECK(esp_wifi_disconnect());

    // Wait for disconnect result
    xEventGroupWaitBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000));

    // Reset event status
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_DISCONNECTED_BIT);

    stop_sntp();

    _is_wifi_connected = false;
}

void Hal::start_sntp()
{
    mclog::tagInfo(_tag, "start sntp");

    if (!_is_wifi_connected) {
        mclog::tagError(_tag, "wifi not connected");
        return;
    }

    // Set timezone to UTC+8
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

void Hal::stop_sntp()
{
    mclog::tagInfo(_tag, "stop sntp");

    if (!_is_wifi_connected) {
        mclog::tagError(_tag, "wifi not connected");
        return;
    }

    esp_sntp_stop();
}
#endif

/* -------------------------------------------------------------------------- */
/*                                   EspNow                                   */
/* -------------------------------------------------------------------------- */
#if defined(ARDUINO)
void Hal::espNowInit()
{
    mclog::tagWarn(_tag, "esp now not supported in this Arduino port");
}

void Hal::espNowDeinit()
{
}

void Hal::espNowSend(const std::string& data)
{
    (void)data;
}

bool Hal::espNowAvailable()
{
    return false;
}

const std::string& Hal::espNowGetReceivedData()
{
    static const std::string empty;
    return empty;
}

void Hal::espNowClearReceivedData()
{
}

#else
// https://github.com/espressif/esp-now/blob/master/examples/get-started/main/app_main.c
#include <esp_mac.h>
#include <espnow.h>
#include <espnow_storage.h>
#include <espnow_utils.h>
#include <esp_check.h>

static std::string _espnow_received_data;
static esp_err_t _handle_espnow_received(uint8_t* src_addr, void* data, size_t size, wifi_pkt_rx_ctrl_t* rx_ctrl)
{
    const char* TAG = "espnow";

    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    static uint32_t count = 0;

    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %.*s", count++, MAC2STR(src_addr),
             rx_ctrl->channel, rx_ctrl->rssi, size, size, (char*)data);

    _espnow_received_data = std::string((char*)data, size);

    return ESP_OK;
}

void Hal::espNowInit()
{
    mclog::tagInfo(_tag, "esp now init");

    if (!_is_wifi_inited) {
        wifiInit();
    }

    if (_is_wifi_connected) {
        wifiDisconnect();
        espNowDeinit();
    }

    if (_is_esp_now_inited) {
        mclog::tagInfo(_tag, "esp now already inited");
        return;
    }

    // espnow_storage_init();

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_init(&espnow_config);
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, _handle_espnow_received);

    _is_esp_now_inited = true;
}

void Hal::espNowDeinit()
{
    mclog::tagInfo(_tag, "esp now deinit");

    if (!_is_esp_now_inited) {
        mclog::tagInfo(_tag, "esp now not inited");
        return;
    }

    espnow_deinit();

    _is_esp_now_inited = false;
}

void Hal::espNowSend(const std::string& data)
{
    mclog::tagInfo(_tag, "esp now send: {}", data);

    if (!_is_esp_now_inited) {
        mclog::tagError(_tag, "esp now not inited");
        return;
    }

    espnow_frame_head_t frame_head = ESPNOW_FRAME_CONFIG_DEFAULT();
    auto ret = espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, data.c_str(), data.size(), &frame_head,
                           portMAX_DELAY);

    if (ret != ESP_OK) {
        mclog::tagError(_tag, "failed to send esp now: {}", esp_err_to_name(ret));
    }
}

bool Hal::espNowAvailable()
{
    return _espnow_received_data.size() > 0;
}

const std::string& Hal::espNowGetReceivedData()
{
    return _espnow_received_data;
}

void Hal::espNowClearReceivedData()
{
    _espnow_received_data.clear();
}
#endif

/* -------------------------------------------------------------------------- */
/*                                     IR                                     */
/* -------------------------------------------------------------------------- */
#if defined(ARDUINO)
void Hal::irInit()
{
    mclog::tagInfo(_tag, "ir init on pin %d", HAL_PIN_IR_TX);

    if (_is_ir_inited) {
        mclog::tagInfo(_tag, "ir already inited");
        return;
    }

    // Configure GPIO pin for IR LED
    pinMode(HAL_PIN_IR_TX, OUTPUT);
    digitalWrite(HAL_PIN_IR_TX, LOW);
    
    _is_ir_inited = true;
}

void Hal::irSend(uint8_t addr, uint8_t cmd)
{
    mclog::tagInfo(_tag, "ir send: addr: {:02X}, cmd: {:02X}", addr, cmd);

    if (!_is_ir_inited) {
        mclog::tagError(_tag, "ir not inited");
        return;
    }

    // NEC format: 9ms leader + 4.5ms space + 32 bits data (LSB first)
    // Bit order: address(8) + ~address(8) + command(8) + ~command(8)
    // Each bit: 560us mark + (560us space for 0, 1690us space for 1)
    
    uint32_t data = addr | ((~addr & 0xFF) << 8) | (cmd << 16) | ((~cmd & 0xFF) << 24);
    
    // Send leading pulse (9ms at 38kHz)
    sendIRPulse(9000, 4500);
    
    // Send 32 bits LSB first
    for (int i = 0; i < 32; i++) {
        if (data & (1UL << i)) {
            // Send bit 1: 560us mark + 1690us space
            sendIRPulse(560, 1690);
        } else {
            // Send bit 0: 560us mark + 560us space
            sendIRPulse(560, 560);
        }
    }
    
    // Send stop bit
    sendIRPulse(560, 0);
    
    mclog::tagInfo(_tag, "ir code sent: 0x{:08X}", data);
}

void Hal::sendIRPulse(uint16_t markTime, uint16_t spaceTime)
{
    // Send mark (38kHz carrier ~26us period)
    uint32_t endTime = micros() + markTime;
    while (micros() < endTime) {
        digitalWrite(HAL_PIN_IR_TX, HIGH);
        delayMicroseconds(13);  // 38kHz = 26us period, 50% duty = 13us on
        digitalWrite(HAL_PIN_IR_TX, LOW);
        delayMicroseconds(13);
    }
    
    // Send space
    if (spaceTime > 0) {
        digitalWrite(HAL_PIN_IR_TX, LOW);
        delayMicroseconds(spaceTime);
    }
}

#else
#include "utils/ir_nec/ir_helper.h"

void Hal::irInit()
{
    mclog::tagInfo(_tag, "ir init");

    if (_is_ir_inited) {
        mclog::tagInfo(_tag, "ir already inited");
        return;
    }

    ir_helper_init((gpio_num_t)HAL_PIN_IR_TX);

    _is_ir_inited = true;
}

void Hal::irSend(uint8_t addr, uint8_t cmd)
{
    mclog::tagInfo(_tag, "ir send: addr: {:02X}, cmd: {:02X}", addr, cmd);

    if (!_is_ir_inited) {
        mclog::tagError(_tag, "ir not inited");
        return;
    }

    ir_helper_send(addr, cmd);
}
#endif

/* -------------------------------------------------------------------------- */
/*                                     BLE                                    */
/* -------------------------------------------------------------------------- */
#include "utils/ble_keyboard_wrapper/ble_keyboard_wrapper.h"

void Hal::bleKeyboardInit()
{
    if (_is_ble_keyboard_inited) {
        mclog::tagWarn(_tag, "ble keyboard already initialized");
        // Just restart advertising if already initialized
        ble_keyboard_wrapper_start_advertising();
        return;
    }

    mclog::tagInfo(_tag, "ble keyboard init");

    // Initialize unified BLE HID wrapper (keyboard + media)
    if (!ble_keyboard_wrapper_init("CardputerADV")) {
        mclog::tagError(_tag, "ble keyboard init failed");
        return;
    }

    // Register keyboard event callback to automatically forward keys
    _ble_keyboard_event_slot_id = keyboard.onKeyEvent.connect(
        [this](const Keyboard::KeyEvent_t& keyEvent) { handle_ble_keyboard_event(keyEvent); });

    _is_ble_keyboard_inited = true;
    mclog::tagInfo(_tag, "ble keyboard init done, auto-forwarding enabled");
}

void Hal::bleKeyboardDeinit()
{
    if (!_is_ble_keyboard_inited) {
        return;
    }

    mclog::tagInfo(_tag, "ble keyboard deinit");

    // Disconnect keyboard event callback
    if (_ble_keyboard_event_slot_id >= 0) {
        keyboard.onKeyEvent.disconnect(_ble_keyboard_event_slot_id);
        _ble_keyboard_event_slot_id = -1;
    }

    ble_keyboard_wrapper_deinit();
    _is_ble_keyboard_inited = false;
}

bool Hal::bleKeyboardIsConnected() const
{
    if (!_is_ble_keyboard_inited) {
        return false;
    }

    return ble_keyboard_wrapper_is_connected();
}

const char* Hal::getBleKeyboardName() const
{
    return ble_keyboard_wrapper_get_device_name();
}

void Hal::bleKeyboardClearBonding()
{
    mclog::tagInfo(_tag, "clearing BLE keyboard bonding data");
    ble_keyboard_wrapper_clear_bonding();
}

void Hal::bleKeyboardStartAdvertising()
{
    if (_is_ble_keyboard_inited) {
        ble_keyboard_wrapper_start_advertising();
    }
}

void Hal::bleKeyboardSendMediaKey(uint16_t usageId, bool pressed)
{
    if (!_is_ble_keyboard_inited || !ble_keyboard_wrapper_is_connected()) {
        return;
    }
    ble_keyboard_wrapper_send_media_key(usageId, pressed);
}

void Hal::handle_ble_keyboard_event(const Keyboard::KeyEvent_t& keyEvent)
{
    // Only forward if BLE keyboard is connected
    if (!bleKeyboardIsConnected()) {
        return;
    }

    // Maintain a list of currently pressed non-modifier keys (up to 6)
    static std::vector<uint8_t> pressedKeys;

    // Update pressedKeys based on event
    if (keyEvent.state) {
        // Key pressed - add if non-modifier
        if (!keyEvent.isModifier && keyEvent.keyCode != KEY_NONE) {
            if (std::find(pressedKeys.begin(), pressedKeys.end(), (uint8_t)keyEvent.keyCode) == pressedKeys.end()) {
                if (pressedKeys.size() < 6) {
                    pressedKeys.push_back((uint8_t)keyEvent.keyCode);
                } else {
                    // if overflow, drop the oldest
                    pressedKeys.erase(pressedKeys.begin());
                    pressedKeys.push_back((uint8_t)keyEvent.keyCode);
                }
            }
        }
    } else {
        // Key released - remove from list if present
        if (!keyEvent.isModifier && keyEvent.keyCode != KEY_NONE) {
            auto it = std::find(pressedKeys.begin(), pressedKeys.end(), (uint8_t)keyEvent.keyCode);
            if (it != pressedKeys.end()) {
                pressedKeys.erase(it);
            }
        }
    }

    // Build 6-key array
    uint8_t keys[6] = {0, 0, 0, 0, 0, 0};
    for (size_t i = 0; i < pressedKeys.size() && i < 6; ++i) {
        keys[i] = pressedKeys[i];
    }

    // Get modifier mask from keyboard state (same as USB handling)
    uint8_t modifiers = GetHAL().keyboard.getModifierMask();

    // Send HID report via BLE (mimic USB behavior)
    if (modifiers == 0 && pressedKeys.empty()) {
        // no keys, send empty report
        ble_keyboard_wrapper_send_report(0, nullptr);
    } else {
        ble_keyboard_wrapper_send_report(modifiers, keys);
    }

    mclog::tagDebug(_tag, "ble keyboard report sent: modifiers=0x{:02x}, keys=[{}]",
                    modifiers, fmt::join(pressedKeys, ","));
}

/* -------------------------------------------------------------------------- */
/*                                     USB                                    */
/* -------------------------------------------------------------------------- */
// https://github.com/espressif/esp-idf/blob/v5.4.2/examples/peripherals/usb/device/tusb_hid
#include "utils/tusb_hid_device/tusb_hid_device_helper.h"

void Hal::usbKeyboardInit()
{
    if (_is_usb_keyboard_inited) {
        mclog::tagWarn(_tag, "usb keyboard already initialized");
        return;
    }

    mclog::tagInfo(_tag, "usb keyboard init");

    delay(200);

    tusb_hid_device_helper_init();

    _usb_keyboard_event_slot_id = keyboard.onKeyEvent.connect(
        [this](const Keyboard::KeyEvent_t& keyEvent) { handle_usb_keyboard_event(keyEvent); });

    _is_usb_keyboard_inited = true;
}

void Hal::usbKeyboardDeinit()
{
    if (!_is_usb_keyboard_inited) {
        return;
    }

    if (_usb_keyboard_event_slot_id >= 0) {
        keyboard.onKeyEvent.disconnect(_usb_keyboard_event_slot_id);
        _usb_keyboard_event_slot_id = -1;
    }

    tusb_hid_device_helper_deinit();
    _is_usb_keyboard_inited = false;
}

void Hal::usbSwitchToSerialJtag()
{
    tusb_hid_device_helper_switch_to_serial_jtag();
}

bool Hal::usbKeyboardIsConnected() const
{
    if (!_is_usb_keyboard_inited) {
        return false;
    }

    return tusb_hid_device_helper_is_mounted();
}

void Hal::handle_usb_keyboard_event(const Keyboard::KeyEvent_t& keyEvent)
{
    // Only forward if USB keyboard is connected
    if (!usbKeyboardIsConnected()) {
        return;
    }

    // Handle key press/release
    if (keyEvent.state) {
        uint8_t keycode[6] = {keyEvent.keyCode};
        tusb_hid_device_helper_report(GetHAL().keyboard.getModifierMask(), keycode);
        mclog::tagDebug(_tag, "usb keyboard sent key: {} (code: {})", keyEvent.keyName ? keyEvent.keyName : "special",
                        (int)keyEvent.keyCode);
    } else {
        tusb_hid_device_helper_report(0, NULL);
        mclog::tagDebug(_tag, "usb keyboard key released");
    }
}

/* -------------------------------------------------------------------------- */
/*                                     SPI                                    */
/* -------------------------------------------------------------------------- */
#if defined(ARDUINO)
static bool _spi_bus_initialized = false;

void Hal::spi_init()
{
    mclog::tagInfo(_tag, "spi init");

    if (_spi_bus_initialized) {
        mclog::tagWarn(_tag, "spi bus already initialized, reusing");
        return;
    }

    SPI.begin(HAL_PIN_SPI_SCLK, HAL_PIN_SPI_MISO, HAL_PIN_SPI_MOSI, HAL_PIN_SD_CARD_CS);
    _spi_bus_initialized = true;
    mclog::tagInfo(_tag, "spi bus initialized");
}

#else
#include <driver/spi_master.h>
#include <driver/sdspi_host.h>
#include <driver/sdmmc_host.h>

static bool _spi_bus_initialized = false;

void Hal::spi_init()
{
    mclog::tagInfo(_tag, "spi init");

    esp_err_t ret;

    // spi_host_device_t host_id = SPI2_HOST;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = HAL_PIN_SPI_MOSI,
        .miso_io_num     = HAL_PIN_SPI_MISO,
        .sclk_io_num     = HAL_PIN_SPI_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };

    // Initialize SPI bus only if not already initialized
    if (!_spi_bus_initialized) {
        ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            mclog::tagError(_tag, "failed to initialize SPI bus");
            return;
        }
        _spi_bus_initialized = true;
        mclog::tagInfo(_tag, "spi bus initialized");
    } else {
        mclog::tagWarn(_tag, "spi bus already initialized, reusing");
    }
}
#endif

/* -------------------------------------------------------------------------- */
/*                                   SD Card                                  */
/* -------------------------------------------------------------------------- */
#if defined(ARDUINO)
void Hal::sd_card_init()
{
    mclog::tagInfo(_tag, "sd card init");

    if (!_spi_bus_initialized) {
        spi_init();
    }

    if (_is_sd_card_mounted) {
        mclog::tagInfo(_tag, "sd card already mounted");
        return;
    }

    if (!SD.begin(HAL_PIN_SD_CARD_CS, SPI)) {
        mclog::tagError(_tag, "failed to initialize sd card");
        return;
    }

    _is_sd_card_mounted = true;
}

Hal::SdCardProbeResult_t Hal::sdCardProbe()
{
    SdCardProbeResult_t result;

    if (!_is_sd_card_mounted) {
        sd_card_init();
        if (!_is_sd_card_mounted) {
            result.is_mounted = false;
            result.size       = "Not Found";
            result.type       = "Type: Unknown";
            result.name       = "Name: Unknown";
            return result;
        }
    }

    result.is_mounted = true;
    result.name       = "Name: Unknown";

    uint8_t card_type = SD.cardType();
    result.type       = "Type: ";
    switch (card_type) {
        case CARD_MMC:
            result.type += "MMC";
            break;
        case CARD_SD:
            result.type += "SDSC";
            break;
        case CARD_SDHC:
            result.type += "SDHC/SDXC";
            break;
        default:
            result.type += "Unknown";
            break;
    }

    uint64_t card_size = SD.cardSize();
    if (card_size > 0) {
        result.size = fmt::format("Size: {:.1f} GB", (double)card_size / (1024.0 * 1024.0 * 1024.0));
    } else {
        result.size = "Size: Unknown";
    }

    if (SD.exists("/test.txt")) {
        SD.remove("/test.txt");
    }
    File fp = SD.open("/test.txt", FILE_WRITE);
    if (fp) {
        fp.print("Hello, World!");
        fp.close();
    } else {
        result.size = "Write Failed";
    }

    return result;
}

#else
// https://github.com/espressif/esp-idf/blob/v5.3.3/examples/storage/sd_card/sdspi
// https://github.com/m5stack/M5PaperS3-UserDemo/blob/main/main/hal/hal.h
#include <driver/spi_master.h>
#include <driver/sdspi_host.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

#define MOUNT_POINT "/sdcard"

static sdmmc_card_t* _sd_card = nullptr;

void Hal::sd_card_init()
{
    mclog::tagInfo(_tag, "sd card init");

    if (!_spi_bus_initialized) {
        spi_init();
    }

    // If already mounted successfully, return
    if (_is_sd_card_mounted) {
        mclog::tagInfo(_tag, "sd card already mounted");
        return;
    }

    esp_err_t ret;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

    const char mount_point[] = MOUNT_POINT;
    mclog::tagInfo(_tag, "initializing SD card");

    // Initialize SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs               = HAL_PIN_SD_CARD_CS;
    slot_config.host_id               = (spi_host_device_t)host.slot;

    mclog::tagInfo(_tag, "mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &_sd_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            mclog::tagError(_tag, "failed to mount filesystem");
        } else {
            mclog::tagError(_tag, "failed to initialize the card, make sure SD card lines have pull-up resistors");
        }

        // Don't clean up SPI bus on failure - leave it for retry
        mclog::tagInfo(_tag, "sd card init failed, but spi bus remains initialized for retry");
        return;
    }

    mclog::tagInfo(_tag, "filesystem mounted successfully");

    sdmmc_card_print_info(stdout, _sd_card);

    _is_sd_card_mounted = true;
}

Hal::SdCardProbeResult_t Hal::sdCardProbe()
{
    SdCardProbeResult_t result;

    if (!_is_sd_card_mounted) {
        sd_card_init();
        if (!_is_sd_card_mounted) {
            result.is_mounted = false;
            result.size       = "Not Found";
            return result;
        }
    }

    result.is_mounted = true;

    // Try write to sd card
    FILE* fp = fopen(MOUNT_POINT "/test.txt", "w");
    if (fp) {
        fwrite("Hello, World!", 1, 13, fp);
        fclose(fp);

        result.size =
            fmt::format("Size: {:.1f} GB",
                        ((float)((uint64_t)_sd_card->csd.capacity) * _sd_card->csd.sector_size) / (1024 * 1024 * 1024));
    } else {
        result.size = "Write Failed";
    }

    result.type = "Type: ";
    if (_sd_card->is_sdio) {
        result.type += "SDIO";
    } else if (_sd_card->is_mmc) {
        result.type += "MMC";
    } else {
        result.type += (_sd_card->ocr & (1 << 30)) ? "SDHC/SDXC" : "SDSC";
    }

    result.name = fmt::format("Name: {}", std::string(_sd_card->cid.name));

    return result;
}
#endif

/* -------------------------------------------------------------------------- */
/*                                   BLE Mouse                                */
/* -------------------------------------------------------------------------- */
#include <hal/utils/ble_mouse_wrapper/ble_mouse_wrapper.h>

static BleMouseWrapper* bleMouse = nullptr; // BLE Mouse instance

void Hal::bleMouseInit() {
    bleMouseInitWithName("CardputerMouse");
}

void Hal::bleMouseInitWithName(const std::string& deviceName) {
    if (_is_ble_mouse_inited) {
        mclog::tagWarn(_tag, "BLE Mouse already initialized");
        return;
    }
    bleMouse = new BleMouseWrapper(deviceName);
    bleMouse->begin();
    _is_ble_mouse_inited = true;
    mclog::tagInfo(_tag, "BLE Mouse initialized with name: {}", deviceName);
}

void Hal::bleMouseDeinit() {
    if (!_is_ble_mouse_inited || !bleMouse) {
        return;
    }
    bleMouse->end();
    delete bleMouse;
    bleMouse = nullptr;
    _is_ble_mouse_inited = false;
    mclog::tagInfo(_tag, "BLE Mouse deinitialized");
}

bool Hal::bleMouseIsConnected() const {
    if (!_is_ble_mouse_inited || !bleMouse) {
        return false;
    }
    return bleMouse->isConnected();
}

void Hal::bleMousePress(uint8_t button) {
    if (bleMouse) {
        bleMouse->press(button);
    }
}

void Hal::bleMouseRelease(uint8_t button) {
    if (bleMouse) {
        bleMouse->release(button);
    }
}

void Hal::bleMouseMove(int x, int y) {
    if (bleMouse) {
        bleMouse->move(x, y);
    }
}

void Hal::bleMouseCenterCursor() {
    if (bleMouse) {
        bleMouse->move(-99999, -99999);
    }
}

const std::string& Hal::getBleMouseName() const {
    static const std::string empty;
    if (bleMouse) {
        return bleMouse->getDeviceName();
    }
    return empty;
}

void Hal::bleMouseClearBonding() {
    mclog::tagInfo(_tag, "Clearing BLE bonding data...");
    BleMouseWrapper::clearBondingData();
    mclog::tagInfo(_tag, "BLE bonding data cleared");
}
