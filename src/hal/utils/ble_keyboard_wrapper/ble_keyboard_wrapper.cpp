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
#include <esp_log.h>
#include <esp_gap_ble_api.h>

static const char* TAG = "BLE_KBD";

// HID Report IDs
#define REPORT_ID_KEYBOARD   1
#define REPORT_ID_CONSUMER   2

// Combined HID Report Descriptor: Keyboard + Consumer Control
static const uint8_t hidReportDescriptor[] = {
    // ============== Keyboard ==============
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_KEYBOARD, //   Report ID (1)
    
    // Modifier keys (8 bits)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0xE0,        //   Usage Minimum (224) - Left Control
    0x29, 0xE7,        //   Usage Maximum (231) - Right GUI
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    
    // Reserved byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)
    
    // LED output report (for host to control Num Lock, Caps Lock, etc.)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x05,        //   Usage Maximum (5)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    
    // LED padding
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Constant)
    
    // Keycodes (6 bytes)
    0x95, 0x06,        //   Report Count (6)
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
    
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array)
    
    0xC0               // End Collection
};

// Global state
static BLEHIDDevice* hid = nullptr;
static BLECharacteristic* inputKeyboard = nullptr;
static BLECharacteristic* inputConsumer = nullptr;
static BLECharacteristic* outputKeyboard = nullptr;
static BLEServer* pServer = nullptr;
static BleHidState_t currentState = BLE_HID_STATE_IDLE;
static char deviceNameBuffer[32] = "CardputerKBD";
static bool isInitialized = false;
static uint8_t batteryLevel = 100;

// Keyboard state
static uint8_t pressedKeys[6] = {0};
static uint8_t modifiers = 0;

// Connection callbacks
class BleKeyboardCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        ESP_LOGI(TAG, "Client connected");
        currentState = BLE_HID_STATE_CONNECTED;
        
        // Stop advertising on connect
        BLEDevice::stopAdvertising();
    }
    
    void onDisconnect(BLEServer* pServer) override {
        ESP_LOGI(TAG, "Client disconnected");
        currentState = BLE_HID_STATE_IDLE;
        
        // Clear keyboard state
        memset(pressedKeys, 0, sizeof(pressedKeys));
        modifiers = 0;
        
        // Restart advertising after a short delay
        delay(100);
        ble_keyboard_wrapper_start_advertising();
    }
};

static BleKeyboardCallbacks serverCallbacks;

// Output report callback (for LED status from host)
class OutputReportCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        uint8_t* data = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();
        if (len > 0) {
            ESP_LOGD(TAG, "LED status: 0x%02X", data[0]);
            // data[0] contains LED state:
            // bit 0: Num Lock
            // bit 1: Caps Lock
            // bit 2: Scroll Lock
            // bit 3: Compose
            // bit 4: Kana
        }
    }
};

static OutputReportCallbacks outputCallbacks;

bool ble_keyboard_wrapper_init(const char* deviceName) {
    if (isInitialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    // Save device name
    strncpy(deviceNameBuffer, deviceName, sizeof(deviceNameBuffer) - 1);
    deviceNameBuffer[sizeof(deviceNameBuffer) - 1] = '\0';
    
    ESP_LOGI(TAG, "Initializing BLE HID: %s", deviceNameBuffer);
    
    // Initialize BLE
    BLEDevice::init(deviceNameBuffer);
    
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
    // Using Espressif's vendor ID
    hid->pnp(0x02, 0x05AC, 0x820A, 0x0001);  // Apple-like for better compatibility
    
    // HID info: country code = 0, flags = 0x02 (normally connectable)
    hid->hidInfo(0x00, 0x02);
    
    // Set security - using NO_BOND for better Windows compatibility
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    
    // Set report map
    hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
    
    // Create input reports
    inputKeyboard = hid->inputReport(REPORT_ID_KEYBOARD);
    inputConsumer = hid->inputReport(REPORT_ID_CONSUMER);
    
    // Create output report (for LED status)
    outputKeyboard = hid->outputReport(REPORT_ID_KEYBOARD);
    outputKeyboard->setCallbacks(&outputCallbacks);
    
    // Start HID services
    hid->startServices();
    
    // Set battery level
    hid->setBatteryLevel(batteryLevel);
    
    // Start advertising
    ble_keyboard_wrapper_start_advertising();
    
    isInitialized = true;
    ESP_LOGI(TAG, "BLE HID initialized successfully");
    
    return true;
}

void ble_keyboard_wrapper_deinit(void) {
    if (!isInitialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing BLE HID");
    
    ble_keyboard_wrapper_stop_advertising();
    
    // Release all keys before deinit
    ble_keyboard_wrapper_release_all();
    
    BLEDevice::deinit(true);
    
    hid = nullptr;
    inputKeyboard = nullptr;
    inputConsumer = nullptr;
    outputKeyboard = nullptr;
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

void ble_keyboard_wrapper_start_advertising(void) {
    if (!isInitialized || !pServer) {
        return;
    }
    
    if (currentState == BLE_HID_STATE_CONNECTED) {
        ESP_LOGD(TAG, "Already connected, not starting advertising");
        return;
    }
    
    ESP_LOGI(TAG, "Starting BLE advertising");
    
    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    
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
    
    ESP_LOGI(TAG, "Advertising started");
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

static void sendKeyboardReport(void) {
    if (!inputKeyboard || currentState != BLE_HID_STATE_CONNECTED) {
        return;
    }
    
    // Keyboard report: [modifiers, reserved, key1, key2, key3, key4, key5, key6]
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[1] = 0;  // Reserved
    memcpy(&report[2], pressedKeys, 6);
    
    inputKeyboard->setValue(report, sizeof(report));
    inputKeyboard->notify();
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
            sendKeyboardReport();
            return;
        }
    }
    
    // No empty slot, replace oldest (first)
    memmove(&pressedKeys[0], &pressedKeys[1], 5);
    pressedKeys[5] = keyCode;
    sendKeyboardReport();
}

void ble_keyboard_wrapper_release(uint8_t keyCode) {
    for (int i = 0; i < 6; i++) {
        if (pressedKeys[i] == keyCode) {
            pressedKeys[i] = 0;
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
        return;
    }
    
    uint8_t report[2] = {0};
    
    if (pressed) {
        report[0] = usageId & 0xFF;
        report[1] = (usageId >> 8) & 0xFF;
    }
    // else report stays all zeros (key release)
    
    inputConsumer->setValue(report, sizeof(report));
    inputConsumer->notify();
}

void ble_keyboard_wrapper_clear_bonding(void) {
    ESP_LOGI(TAG, "Clearing BLE bonding data");
    
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t* dev_list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (dev_list) {
            esp_ble_get_bond_device_list(&dev_num, dev_list);
            for (int i = 0; i < dev_num; i++) {
                esp_ble_remove_bond_device(dev_list[i].bd_addr);
                ESP_LOGI(TAG, "Removed bonded device %d", i);
            }
            free(dev_list);
        }
    }
    
    ESP_LOGI(TAG, "Bonding data cleared (%d devices)", dev_num);
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
