/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "tusb_hid_device_helper.h"

#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDKeyboard.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "esp_intr_alloc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "soc/usb_periph.h"
#include "hal/usb_serial_jtag_ll.h"
#if defined(CONFIG_TINYUSB_ENABLED) && CONFIG_TINYUSB_ENABLED
#include "tusb.h"
#endif

static const char* TAG = "tusb_hid";

#if defined(CONFIG_TINYUSB_ENABLED) && CONFIG_TINYUSB_ENABLED && defined(CONFIG_TINYUSB_HID_ENABLED) && \
    CONFIG_TINYUSB_HID_ENABLED
static bool s_usb_started = false;
static USBHIDKeyboard s_keyboard;
static USBCDC s_cdc;

#if CONFIG_IDF_TARGET_ESP32S3
static void hw_cdc_reset_handler(void* arg)
{
    BaseType_t xTaskWoken          = pdFALSE;
    uint32_t usbjtag_intr_status = usb_serial_jtag_ll_get_intsts_mask();
    usb_serial_jtag_ll_clr_intsts_mask(usbjtag_intr_status);

    if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_BUS_RESET) {
        xSemaphoreGiveFromISR((SemaphoreHandle_t)arg, &xTaskWoken);
    }

    if (xTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void usb_switch_to_cdc_jtag()
{
    periph_module_reset(PERIPH_USB_MODULE);
    periph_module_disable(PERIPH_USB_MODULE);

    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_IO_MUX_RESET_DISABLE);
    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_USB_RESET_DISABLE);

    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG,
                        (RTC_CNTL_SW_HW_USB_PHY_SEL | RTC_CNTL_SW_USB_PHY_SEL | RTC_CNTL_USB_PAD_ENABLE));

    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PHY_SEL);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);

    SET_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL);
    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_USB_PHY_SEL);

    pinMode(USBPHY_DM_NUM, OUTPUT_OPEN_DRAIN);
    pinMode(USBPHY_DP_NUM, OUTPUT_OPEN_DRAIN);
    digitalWrite(USBPHY_DM_NUM, LOW);
    digitalWrite(USBPHY_DP_NUM, LOW);
    delay(200);

    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_BUS_RESET);

    intr_handle_t intr_handle      = NULL;
    SemaphoreHandle_t reset_sem = xSemaphoreCreateBinary();
    if (reset_sem) {
        if (esp_intr_alloc(ETS_USB_SERIAL_JTAG_INTR_SOURCE, 0, hw_cdc_reset_handler, reset_sem, &intr_handle) !=
            ESP_OK) {
            vSemaphoreDelete(reset_sem);
            reset_sem = NULL;
            ESP_LOGE(TAG, "HW USB CDC failed to init interrupts");
        }
    } else {
        ESP_LOGE(TAG, "reset_sem init failed");
    }

    pinMode(USBPHY_DM_NUM, INPUT);
    pinMode(USBPHY_DP_NUM, INPUT);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);

    if (reset_sem) {
        if (xSemaphoreTake(reset_sem, pdMS_TO_TICKS(1000)) != pdPASS) {
            ESP_LOGE(TAG, "reset_sem timeout");
        }
        usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
        esp_intr_free(intr_handle);
        vSemaphoreDelete(reset_sem);
    }
}
#endif

extern "C" void tusb_hid_device_helper_init(void)
{
    if (s_usb_started) {
        tud_connect();
        return;
    }

    s_cdc.begin();

    if (!USB.begin()) {
        ESP_LOGE(TAG, "USB begin failed");
        return;
    }

    s_keyboard.begin();
    s_usb_started = true;
    ESP_LOGI(TAG, "USB HID keyboard started");
}

extern "C" void tusb_hid_device_helper_deinit(void)
{
    if (!s_usb_started) {
        return;
    }

    s_keyboard.releaseAll();
    tud_disconnect();

    s_usb_started = false;
    ESP_LOGI(TAG, "USB HID keyboard stopped");
}

extern "C" void tusb_hid_device_helper_report(uint8_t modifier, uint8_t* keycode)
{
    if (!s_usb_started) {
        return;
    }

    KeyReport report = {};
    report.modifiers = modifier;
    if (keycode) {
        report.keys[0] = keycode[0];
    }
    s_keyboard.sendReport(&report);
}

extern "C" bool tusb_hid_device_helper_is_mounted(void)
{
    return s_usb_started && USB;
}

extern "C" void tusb_hid_device_helper_switch_to_serial_jtag(void)
{
#if CONFIG_IDF_TARGET_ESP32S3
    usb_switch_to_cdc_jtag();
#else
    ESP_LOGW(TAG, "USB Serial/JTAG switch not supported on this target");
#endif
}
#else
extern "C" void tusb_hid_device_helper_init(void)
{
    ESP_LOGW(TAG, "TinyUSB HID not enabled; USB HID disabled in this build");
}

extern "C" void tusb_hid_device_helper_deinit(void)
{
}

extern "C" void tusb_hid_device_helper_report(uint8_t modifier, uint8_t* keycode)
{
    (void)modifier;
    (void)keycode;
}

extern "C" bool tusb_hid_device_helper_is_mounted(void)
{
    return false;
}

extern "C" void tusb_hid_device_helper_switch_to_serial_jtag(void)
{
    ESP_LOGW(TAG, "USB Serial/JTAG switch not supported in this build");
}
#endif
