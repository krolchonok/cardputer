/* Linker stubs for BLE HID helper functions — placed in src/hal so CMake picks it up.
 * These are minimal no-op implementations to satisfy linker when full BLE HID
 * implementation (in nested folders) is not compiled.
 */
#include "hal/utils/ble_hid_device/ble_hid_device_helper.h"

bool __attribute__((weak)) ble_hid_device_helper_init(void)
{
    return false;
}

void __attribute__((weak)) ble_hid_device_helper_start_advertising(void)
{
}

BleHidDeviceState_t __attribute__((weak)) ble_hid_device_helper_get_state(void)
{
    return BLE_HID_DEVICE_STATE_IDLE;
}

const char* __attribute__((weak)) ble_hid_device_helper_get_device_name(void)
{
    return "BLE-HID-Stub";
}

void __attribute__((weak)) esp_hidd_send_consumer_value(uint8_t key_cmd, bool key_pressed)
{
    (void)key_cmd;
    (void)key_pressed;
}
