/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "usb_msc_device.h"
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>
#include <USB.h>
#include <SD.h>
#include <SPI.h>

// Access external HID helper functions
extern "C" void tusb_hid_device_helper_deinit(void);
extern "C" void tusb_hid_device_helper_init(void);

#if ARDUINO_USB_MSC_ON_BOOT
#include <USBMSC.h>

static USBMSC MSC;

// MSC callbacks for SD card access
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    return SD.writeRAW((uint8_t*)buffer, lba) ? bufsize : -1;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    return SD.readRAW((uint8_t*)buffer, lba) ? bufsize : -1;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    ESP_LOGI("usb_msc_device", "MSC Start/Stop: power=%d, start=%d, eject=%d", 
             power_condition, start, load_eject);
    return true;
}
#endif

static const char* TAG = "usb_msc_device";
static bool _device_initialized = false;
static bool _device_active = false;

bool usb_msc_device_init(void)
{
    if (_device_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

#if ARDUINO_USB_MSC_ON_BOOT
    ESP_LOGI(TAG, "USB MSC support available");
    _device_initialized = true;
    return true;
#else
    ESP_LOGI(TAG, "USB MSC support not enabled (rebuild with USB MSC)");
    _device_initialized = true;
    return true;
#endif
}


void usb_msc_device_deinit(void)
{
    if (!_device_initialized) {
        return;
    }

    if (_device_active) {
        usb_msc_device_stop();
    }

    ESP_LOGI(TAG, "USB MSC device deinitialized");
    _device_initialized = false;
}

bool usb_msc_device_start(void)
{
    if (!_device_initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return false;
    }

    if (_device_active) {
        ESP_LOGW(TAG, "Already active");
        return true;
    }

#if ARDUINO_USB_MSC_ON_BOOT
    ESP_LOGI(TAG, "Starting USB MSC mode");
    
    // Step 1: Disable HID/CDC USB
    ESP_LOGI(TAG, "Disabling USB HID/CDC...");
    tusb_hid_device_helper_deinit();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Step 2: Check if SD card is available
    if (!SD.cardSize()) {
        ESP_LOGE(TAG, "SD card not available");
        // Restore HID
        vTaskDelay(pdMS_TO_TICKS(200));
        tusb_hid_device_helper_init();
        return false;
    }
    
    // Step 3: Start USB MSC
    ESP_LOGI(TAG, "Initializing USB MSC...");
    
    MSC.vendorID("ESP32");
    MSC.productID("SD Card");
    MSC.productRevision("1.0");
    MSC.onRead(onRead);
    MSC.onWrite(onWrite);
    MSC.onStartStop(onStartStop);
    MSC.mediaPresent(true);
    
    uint32_t sector_count = SD.cardSize() / 512;
    uint32_t sector_size = 512;
    
    ESP_LOGI(TAG, "SD Card: %lu sectors x %lu bytes", sector_count, sector_size);
    
    MSC.begin(sector_count, sector_size);
    
    _device_active = true;
    ESP_LOGI(TAG, "USB MSC mode activated - SD card exposed to host");
    return true;
#else
    ESP_LOGW(TAG, "USB MSC not available - requires ARDUINO_USB_MSC_ON_BOOT");
    return false;
#endif
}

bool usb_msc_device_stop(void)
{
    if (!_device_initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return false;
    }

    if (!_device_active) {
        ESP_LOGW(TAG, "Not active");
        return true;
    }

#if ARDUINO_USB_MSC_ON_BOOT
    ESP_LOGI(TAG, "Stopping USB MSC mode");
    
    // Step 1: End MSC
    MSC.end();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Step 2: Restore HID/CDC USB
    ESP_LOGI(TAG, "Restoring USB HID/CDC...");
    tusb_hid_device_helper_init();
    
    _device_active = false;
    ESP_LOGI(TAG, "USB MSC mode deactivated - HID/CDC restored");
    return true;
#else
    ESP_LOGW(TAG, "USB MSC not available");
    return false;
#endif
}

bool usb_msc_device_is_active(void)
{
    return _device_active;
}
