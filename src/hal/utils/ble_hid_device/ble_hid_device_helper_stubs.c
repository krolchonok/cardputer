/* Minimal stubs to satisfy linker when BLE HID helpers are not compiled.
 * These are no-op implementations so the media UI can compile.
 */
#include "ble_hid_device_helper.h"

bool ble_hid_device_helper_init(void)
{
    return false;
}

void ble_hid_device_helper_start_advertising(void)
{
    // no-op
}

BleHidDeviceState_t ble_hid_device_helper_get_state(void)
{
    return BLE_HID_DEVICE_STATE_IDLE;
}

const char* ble_hid_device_helper_get_device_name(void)
{
    return "BLE-HID-Stub";
}

void esp_hidd_send_consumer_value(uint8_t key_cmd, bool key_pressed)
{
    // no-op stub
    (void)key_cmd;
    (void)key_pressed;
}
