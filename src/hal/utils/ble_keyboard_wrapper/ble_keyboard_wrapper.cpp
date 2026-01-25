/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 * 
 * Unified BLE HID implementation for Keyboard + Media Control
 * Based on ESP32 BLE stack (no external libraries)
 */
#include "ble_keyboard_wrapper.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>

#include <string.h>
#include <Arduino.h>
#include <esp_gap_ble_api.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>

static const char* TAG = "ble_kbd";

// HID Report IDs
#define REPORT_ID_KEYBOARD   1
#define REPORT_ID_CONSUMER   2

// Combined HID Report Descriptor: Keyboard + Consumer Control
// Match the proven ESP32 BLE Keyboard descriptor layout for compatibility.
static const uint8_t hidReportDescriptor[] = {
    // ============== Keyboard ==============
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_KEYBOARD, //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0xE0,        //   Usage Minimum (224) - Left Control
    0x29, 0xE7,        //   Usage Maximum (231) - Right GUI
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1) ; Reserved
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)
    0x95, 0x05,        //   Report Count (5) ; LEDs
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x05,        //   Usage Maximum (5)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1) ; LED padding
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Constant)
    0x95, 0x06,        //   Report Count (6) ; Keycodes
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0,              // End Collection

    // ============== Consumer Control (Media Keys) ==============
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_CONSUMER, //   Report ID (2)
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x09, 0xB5,        //   Usage (Scan Next Track) bit0
    0x09, 0xB6,        //   Usage (Scan Previous Track) bit1
    0x09, 0xB7,        //   Usage (Stop) bit2
    0x09, 0xCD,        //   Usage (Play/Pause) bit3
    0x09, 0xE2,        //   Usage (Mute) bit4
    0x09, 0xE9,        //   Usage (Volume Increment) bit5
    0x09, 0xEA,        //   Usage (Volume Decrement) bit6
    0x0A, 0x23, 0x02,  //   Usage (WWW Home) bit7
    0x0A, 0x94, 0x01,  //   Usage (My Computer) bit8
    0x0A, 0x92, 0x01,  //   Usage (Calculator) bit9
    0x0A, 0x2A, 0x02,  //   Usage (WWW fav) bit10
    0x0A, 0x21, 0x02,  //   Usage (WWW search) bit11
    0x0A, 0x26, 0x02,  //   Usage (WWW stop) bit12
    0x0A, 0x24, 0x02,  //   Usage (WWW back) bit13
    0x0A, 0x83, 0x01,  //   Usage (Media select) bit14
    0x0A, 0x8A, 0x01,  //   Usage (Mail) bit15
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0xC0               // End Collection
};

// Global state
static BLEHIDDevice* hid = nullptr;
static BLECharacteristic* inputKeyboard = nullptr;
static BLECharacteristic* inputConsumer = nullptr;
static BLECharacteristic* outputKeyboard = nullptr;
static BLECharacteristic* bootKeyboard = nullptr;
static BLE2902* inputKeyboardCccd = nullptr;
static BLE2902* inputConsumerCccd = nullptr;
static BLEServer* pServer = nullptr;
static BleHidState_t currentState = BLE_HID_STATE_IDLE;
static char deviceNameBuffer[32] = "CardputerKBD";
static bool isInitialized = false;
static uint8_t batteryLevel = 100;
static bool autoAdvertise = true;
static bool allowConnections = true;
static uint32_t lastDisconnectMs = 0;
static uint8_t disconnectBurst = 0;
static uint8_t lastRandAddr[6] = {0};
static bool hasRandAddr = false;

static void compute_ble_addr_from_base(uint8_t* out_addr) {
    uint64_t mac64 = ESP.getEfuseMac();
    for (int i = 0; i < 6; i++) {
        out_addr[5 - i] = (mac64 >> (8 * i)) & 0xFF;
    }
    uint64_t macVal = 0;
    for (int i = 0; i < 6; i++) {
        macVal = (macVal << 8) | out_addr[i];
    }
    macVal += 2;
    for (int i = 5; i >= 0; i--) {
        out_addr[i] = macVal & 0xFF;
        macVal >>= 8;
    }
}

class BleSecurityCallbacks : public BLESecurityCallbacks {
public:
    uint32_t onPassKeyRequest() override { return 0; }
    void onPassKeyNotify(uint32_t pass_key) override { (void)pass_key; }
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override
    {
        (void)cmpl;
        // Avoid BLE calls/logging here; some stacks crash when invoked from auth callback.
    }
    bool onConfirmPIN(uint32_t pin) override
    {
        (void)pin;
        return true;
    }
};

static BleSecurityCallbacks securityCallbacks;

// Keyboard state
static uint8_t pressedKeys[6] = {0};
static uint8_t modifiers = 0;
static uint16_t lastConnId = 0xFFFF;

static void log_key_state(const char* reason);

// Forward declaration for delayed CCCD enable
static void enableNotificationsDelayed(void* param);

// Connection callbacks
class BleKeyboardCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        if (!allowConnections) {
            mclog::tagWarn(TAG, "connection rejected (disabled)");
            pServer->disconnect(pServer->getConnId());
            currentState = BLE_HID_STATE_IDLE;
            return;
        }
        uint16_t connId = pServer->getConnId();
        if (connId == lastConnId && currentState == BLE_HID_STATE_CONNECTED) {
            return;
        }
        lastConnId = connId;
        mclog::tagInfo(TAG, "client connected: conn_id={} allow={} auto_adv={}", connId,
                       allowConnections ? 1 : 0, autoAdvertise ? 1 : 0);
        currentState = BLE_HID_STATE_CONNECTED;
        disconnectBurst = 0;

        // For bonded devices reconnecting after reboot, the host won't re-enable notifications
        // because it thinks they are still enabled. We need to force-enable them on our side.
        // Use a short delay to let the connection stabilize before enabling.
        xTaskCreate([](void* param) {
            vTaskDelay(pdMS_TO_TICKS(500));  // Wait for connection to stabilize
            
            if (currentState != BLE_HID_STATE_CONNECTED) {
                vTaskDelete(NULL);
                return;
            }
            
            mclog::tagInfo(TAG, "enabling notifications for reconnected device");
            
            // Force enable notifications on keyboard input
            if (inputKeyboardCccd) {
                inputKeyboardCccd->setNotifications(true);
            }
            
            // Force enable notifications on consumer input
            if (inputConsumerCccd) {
                inputConsumerCccd->setNotifications(true);
            }
            
            // Force enable notifications on boot keyboard
            if (bootKeyboard) {
                BLE2902* bootCccd = (BLE2902*)bootKeyboard->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
                if (bootCccd) {
                    bootCccd->setNotifications(true);
                }
            }
            
            vTaskDelete(NULL);
        }, "ble_cccd_init", 4096, NULL, 1, NULL);
        
        // Stop advertising on connect
        BLEDevice::stopAdvertising();
    }

    
    void onDisconnect(BLEServer* pServer) override {
        mclog::tagInfo(TAG, "client disconnected: conn_id={}", pServer ? pServer->getConnId() : 0xFFFF);
        currentState = BLE_HID_STATE_IDLE;
        lastConnId = 0xFFFF;
        
        // Clear keyboard state
        memset(pressedKeys, 0, sizeof(pressedKeys));
        modifiers = 0;
        
        uint32_t now = millis();
        if (now - lastDisconnectMs < 2000) {
            disconnectBurst++;
        } else {
            disconnectBurst = 1;
        }
        lastDisconnectMs = now;

        if (!allowConnections) {
            return;
        }
        if (autoAdvertise) {
            if (disconnectBurst >= 3) {
                autoAdvertise = false;
                mclog::tagWarn(TAG, "auto advertising paused (repeated disconnects)");
                return;
            }
            // Restart advertising after a short delay
            vTaskDelay(pdMS_TO_TICKS(200));
            ble_keyboard_wrapper_start_advertising();
        }
    }
};

static BleKeyboardCallbacks serverCallbacks;

static void log_key_state(const char* reason)
{
    mclog::tagDebug(TAG, "key state {}: mod=0x{:02X} keys={:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
                    reason ? reason : "",
                    modifiers,
                    pressedKeys[0], pressedKeys[1], pressedKeys[2],
                    pressedKeys[3], pressedKeys[4], pressedKeys[5]);
}

class InputReportCallbacks : public BLECharacteristicCallbacks {
    void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) override
    {
        (void)pCharacteristic;
        if (s == BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY ||
            s == BLECharacteristicCallbacks::Status::SUCCESS_INDICATE) {
            return;
        }
        mclog::tagWarn(TAG, "notify status: s={} code={}", static_cast<int>(s), code);
    }
};

static InputReportCallbacks inputReportCallbacks;

// Output report callback (for LED status from host)
class OutputReportCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        uint8_t* data = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();
        (void)data;
        (void)len;
        // Avoid logging here; some BLE stacks call this in BTC task context.
    }
};

static OutputReportCallbacks outputCallbacks;

bool ble_keyboard_wrapper_init(const char* deviceName) {
    if (isInitialized) {
        mclog::tagWarn(TAG, "already initialized");
        return true;
    }
    
    // Save device name
    strncpy(deviceNameBuffer, deviceName, sizeof(deviceNameBuffer) - 1);
    deviceNameBuffer[sizeof(deviceNameBuffer) - 1] = '\0';
    
    mclog::tagInfo(TAG, "initializing BLE HID: {}", deviceNameBuffer);
    
    // Initialize BLE
    BLEDevice::init(deviceNameBuffer);
    allowConnections = true;
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(&securityCallbacks);
    
    // Set power level for better range
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);
    
    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);
    
    // Create HID Device
    hid = new BLEHIDDevice(pServer);
    
    // Set manufacturer
    hid->manufacturer()->setValue("M5Stack");
    
    // Set PnP info (vendor ID, product ID, version)
    // Use generic BLE keyboard IDs for broad compatibility.
    hid->pnp(0x02, 0xE502, 0xA111, 0x0100);
    
    // HID info: country code = 0, flags = 0x01 (remote wake)
    hid->hidInfo(0x00, 0x01);
    
    // Set security - bonding for HID compatibility
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setKeySize(16);
    
    // Set report map
    hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
    
    // Create input reports (BLEHIDDevice::inputReport already creates BLE2902 descriptors)
    inputKeyboard = hid->inputReport(REPORT_ID_KEYBOARD);
    inputConsumer = hid->inputReport(REPORT_ID_CONSUMER);
    
    // Get the automatically created CCCD descriptors
    inputKeyboardCccd = (BLE2902*)inputKeyboard->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    inputConsumerCccd = (BLE2902*)inputConsumer->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    
    inputKeyboard->setCallbacks(&inputReportCallbacks);
    inputConsumer->setCallbacks(&inputReportCallbacks);
    bootKeyboard = hid->bootInput();
    
    // Create output report (for LED status)
    outputKeyboard = hid->outputReport(REPORT_ID_KEYBOARD);
    // Output report is a single LED byte (no Report ID in payload).
    uint8_t zero_out = 0;
    outputKeyboard->setValue(&zero_out, 1);
    outputKeyboard->setCallbacks(&outputCallbacks);
    
    // Start HID services
    hid->startServices();
    
    // Set battery level
    hid->setBatteryLevel(batteryLevel);
    
    // Start advertising
    ble_keyboard_wrapper_start_advertising();
    
    isInitialized = true;
    mclog::tagInfo(TAG, "BLE HID initialized successfully");
    
    return true;
}

void ble_keyboard_wrapper_deinit(void) {
    if (!isInitialized) {
        return;
    }
    
    mclog::tagInfo(TAG, "deinitializing BLE HID");
    
    ble_keyboard_wrapper_stop_advertising();
    autoAdvertise = false;
    allowConnections = false;
    
    // Release all keys before deinit
    ble_keyboard_wrapper_release_all();

    // Give BLE stack time to quiesce before deinit
    vTaskDelay(pdMS_TO_TICKS(120));
    BLEDevice::deinit(true);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    hid = nullptr;
    inputKeyboard = nullptr;
    inputConsumer = nullptr;
    outputKeyboard = nullptr;
    bootKeyboard = nullptr;
    inputKeyboardCccd = nullptr;
    inputConsumerCccd = nullptr;
    pServer = nullptr;
    currentState = BLE_HID_STATE_IDLE;
    isInitialized = false;
}

bool ble_keyboard_wrapper_is_connected(void) {
    return currentState == BLE_HID_STATE_CONNECTED;
}

BleHidState_t ble_keyboard_wrapper_get_state(void) {
    return currentState;
}

void ble_keyboard_wrapper_set_auto_advertise(bool enabled) {
    autoAdvertise = enabled;
    if (enabled) {
        disconnectBurst = 0;
    }
}

bool ble_keyboard_wrapper_get_auto_advertise(void) {
    return autoAdvertise;
}

void ble_keyboard_wrapper_set_allow_connections(bool enabled) {
    allowConnections = enabled;
}

bool ble_keyboard_wrapper_get_allow_connections(void) {
    return allowConnections;
}

bool ble_keyboard_wrapper_rotate_address(void) {
    if (!isInitialized) {
        return false;
    }

    if (currentState == BLE_HID_STATE_CONNECTED && pServer) {
        pServer->disconnect(pServer->getConnId());
        currentState = BLE_HID_STATE_IDLE;
    }
    BLEDevice::stopAdvertising();
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t addr[6] = {0};
    esp_fill_random(addr, sizeof(addr));
    // Static random address: two MSBs of addr[5] must be 1
    addr[5] = (addr[5] & 0x3F) | 0xC0;

    esp_err_t err = esp_ble_gap_set_rand_addr(addr);
    if (err != ESP_OK) {
        mclog::tagError(TAG, "set rand addr failed: {}", esp_err_to_name(err));
        return false;
    }

    memcpy(lastRandAddr, addr, sizeof(addr));
    hasRandAddr = true;
    if (pServer) {
        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        if (pAdvertising) {
            pAdvertising->setDeviceAddress(addr, BLE_ADDR_TYPE_RANDOM);
        }
    }
    mclog::tagInfo(TAG, "random addr set: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                   addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return true;
}

bool ble_keyboard_wrapper_get_address(uint8_t* out_addr, bool* is_random) {
    if (!out_addr) {
        return false;
    }
    if (hasRandAddr) {
        memcpy(out_addr, lastRandAddr, 6);
        if (is_random) {
            *is_random = true;
        }
        return true;
    }
    compute_ble_addr_from_base(out_addr);
    if (is_random) {
        *is_random = false;
    }
    return true;
}

void ble_keyboard_wrapper_set_saved_address(const uint8_t* addr) {
    if (!addr) {
        return;
    }
    memcpy(lastRandAddr, addr, 6);
    hasRandAddr = true;
}

void ble_keyboard_wrapper_start_advertising(void) {
    if (!isInitialized || !pServer) {
        mclog::tagWarn(TAG, "start advertising skipped: inited={} server={}", isInitialized ? 1 : 0,
                       pServer ? 1 : 0);
        return;
    }
    
    if (currentState == BLE_HID_STATE_CONNECTED) {
        mclog::tagDebug(TAG, "already connected, not starting advertising");
        return;
    }
    if (!allowConnections) {
        mclog::tagWarn(TAG, "advertising blocked (connections disabled)");
        return;
    }

    allowConnections = true;
    
    mclog::tagInfo(TAG, "starting BLE advertising");
    
    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    if (hasRandAddr) {
        esp_err_t err = esp_ble_gap_set_rand_addr(lastRandAddr);
        if (err == ESP_OK) {
            pAdvertising->setDeviceAddress(lastRandAddr, BLE_ADDR_TYPE_RANDOM);
            mclog::tagInfo(TAG, "adv using random addr {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                           lastRandAddr[5], lastRandAddr[4], lastRandAddr[3],
                           lastRandAddr[2], lastRandAddr[1], lastRandAddr[0]);
        } else {
            mclog::tagError(TAG, "set rand addr failed (adv): {}", esp_err_to_name(err));
            hasRandAddr = false;
        }
    }
    
    // Set appearance to keyboard
    pAdvertising->setAppearance(0x03C1);  // Keyboard
    
    // Add HID service UUID
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    
    // Set advertising parameters for better discovery
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // 7.5ms
    pAdvertising->setMaxPreferred(0x12);  // 22.5ms
    
    pAdvertising->start();
    currentState = BLE_HID_STATE_ADVERTISING;
    
    mclog::tagInfo(TAG, "advertising started");
}

void ble_keyboard_wrapper_stop_advertising(void) {
    if (!isInitialized) {
        return;
    }
    
    BLEDevice::stopAdvertising();
    
    if (currentState == BLE_HID_STATE_ADVERTISING) {
        currentState = BLE_HID_STATE_IDLE;
    }
}

void ble_keyboard_wrapper_disconnect(void) {
    if (!isInitialized || !pServer) {
        return;
    }

    autoAdvertise = false;
    allowConnections = false;
    ble_keyboard_wrapper_stop_advertising();
    ble_keyboard_wrapper_release_all();

    if (currentState == BLE_HID_STATE_CONNECTED) {
        uint16_t conn_id = pServer->getConnId();
        pServer->disconnect(conn_id);
        currentState = BLE_HID_STATE_IDLE;
    }
}

static void sendKeyboardReport(void) {
    if (!inputKeyboard || currentState != BLE_HID_STATE_CONNECTED) {
        return;
    }
    
    // Keyboard report: [modifiers, reserved, key1..key6]
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[1] = 0;  // Reserved
    memcpy(&report[2], pressedKeys, 6);
    
    // Always set the value (for hosts that read directly)
    inputKeyboard->setValue(report, sizeof(report));
    
    // Try to notify - this will work if notifications are enabled
    inputKeyboard->notify();

    // Also send via boot keyboard protocol for maximum compatibility
    if (bootKeyboard) {
        bootKeyboard->setValue(report, sizeof(report));
        bootKeyboard->notify();
    }
}

void ble_keyboard_wrapper_press(uint8_t keyCode) {
    // Check if key is already pressed
    for (int i = 0; i < 6; i++) {
        if (pressedKeys[i] == keyCode) {
            return;  // Already pressed
        }
    }
    
    // Find empty slot
    for (int i = 0; i < 6; i++) {
        if (pressedKeys[i] == 0) {
            pressedKeys[i] = keyCode;
            log_key_state("press");
            sendKeyboardReport();
            return;
        }
    }
    
    // No empty slot, replace oldest (first)
    memmove(&pressedKeys[0], &pressedKeys[1], 5);
    pressedKeys[5] = keyCode;
    log_key_state("press overflow");
    sendKeyboardReport();
}

void ble_keyboard_wrapper_release(uint8_t keyCode) {
    for (int i = 0; i < 6; i++) {
        if (pressedKeys[i] == keyCode) {
            pressedKeys[i] = 0;
            log_key_state("release");
            sendKeyboardReport();
            return;
        }
    }
}

void ble_keyboard_wrapper_release_all(void) {
    memset(pressedKeys, 0, sizeof(pressedKeys));
    modifiers = 0;
    sendKeyboardReport();
}

void ble_keyboard_wrapper_send_report(uint8_t mod, const uint8_t* keys) {
    if (!inputKeyboard || currentState != BLE_HID_STATE_CONNECTED) {
        return;
    }
    
    modifiers = mod;
    
    if (keys) {
        memcpy(pressedKeys, keys, 6);
    } else {
        memset(pressedKeys, 0, sizeof(pressedKeys));
    }
    
    sendKeyboardReport();
}

void ble_keyboard_wrapper_send_media_key(uint16_t usageId, bool pressed) {
    if (!inputConsumer || currentState != BLE_HID_STATE_CONNECTED) {
        mclog::tagDebug(TAG, "send media skipped: input={} state={} usage=0x{:04X} pressed={}",
                        inputConsumer ? 1 : 0, static_cast<int>(currentState),
                        usageId, pressed ? 1 : 0);
        return;
    }
    
    uint16_t mask = 0;
    if (pressed) {
        switch (usageId) {
            case 0x00B5: mask = 1u << 0; break; // Next Track
            case 0x00B6: mask = 1u << 1; break; // Previous Track
            case 0x00B7: mask = 1u << 2; break; // Stop
            case 0x00CD: mask = 1u << 3; break; // Play/Pause
            case 0x00E2: mask = 1u << 4; break; // Mute
            case 0x00E9: mask = 1u << 5; break; // Volume Up
            case 0x00EA: mask = 1u << 6; break; // Volume Down
            case 0x0223: mask = 1u << 7; break; // WWW Home
            case 0x0194: mask = 1u << 8; break; // My Computer
            case 0x0192: mask = 1u << 9; break; // Calculator
            case 0x022A: mask = 1u << 10; break; // WWW Favorites
            case 0x0221: mask = 1u << 11; break; // WWW Search
            case 0x0226: mask = 1u << 12; break; // WWW Stop
            case 0x0224: mask = 1u << 13; break; // WWW Back
            case 0x0183: mask = 1u << 14; break; // Media Select
            case 0x018A: mask = 1u << 15; break; // Mail
            default: mask = 0; break;
        }
    }

    uint8_t report[2] = {
        static_cast<uint8_t>(mask & 0xFF),
        static_cast<uint8_t>((mask >> 8) & 0xFF)
    };
    inputConsumer->setValue(report, sizeof(report));
    inputConsumer->notify();
}

void ble_keyboard_wrapper_clear_bonding(void) {
    mclog::tagInfo(TAG, "clearing BLE bonding data");
    
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t* dev_list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (dev_list) {
            esp_ble_get_bond_device_list(&dev_num, dev_list);
            for (int i = 0; i < dev_num; i++) {
                esp_ble_remove_bond_device(dev_list[i].bd_addr);
                mclog::tagInfo(TAG, "removed bonded device {}", i);
            }
            free(dev_list);
        }
    }
    
    mclog::tagInfo(TAG, "bonding data cleared ({} devices)", dev_num);
}

const char* ble_keyboard_wrapper_get_device_name(void) {
    return deviceNameBuffer;
}

void ble_keyboard_wrapper_set_battery_level(uint8_t level) {
    batteryLevel = level;
    if (hid) {
        hid->setBatteryLevel(level);
    }
}
