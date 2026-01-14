/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "usb_msc_helper.h"
#include <usb_msc_device.h>
#include <mooncake_log.h>

static const char* TAG = "usb_msc";
static bool _initialized = false;

void usb_msc_helper_init(void)
{
    if (_initialized) {
        mclog::tagWarn(TAG, "Already initialized");
        return;
    }

    if (usb_msc_device_init()) {
        mclog::tagInfo(TAG, "USB MSC helper initialized");
        _initialized = true;
    } else {
        mclog::tagError(TAG, "Failed to initialize USB MSC device");
    }
}

void usb_msc_helper_deinit(void)
{
    if (_initialized) {
        usb_msc_device_deinit();
        _initialized = false;
        mclog::tagInfo(TAG, "USB MSC helper deinitialized");
    }
}

bool usb_msc_helper_is_active(void)
{
    return usb_msc_device_is_active();
}

bool usb_msc_helper_toggle(void)
{
    if (!_initialized) {
        mclog::tagWarn(TAG, "Not initialized");
        return false;
    }

    if (usb_msc_helper_is_active()) {
        mclog::tagInfo(TAG, "Stopping USB MSC...");
        return usb_msc_device_stop();
    } else {
        mclog::tagInfo(TAG, "Starting USB MSC...");
        return usb_msc_device_start();
    }
}
