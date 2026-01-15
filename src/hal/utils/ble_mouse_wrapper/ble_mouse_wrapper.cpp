#include "ble_mouse_wrapper.h"
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <nvs_flash.h>

BleMouseWrapper::BleMouseWrapper(const std::string& deviceName)
    : _bleMouse(deviceName, "M5Stack", 100), _deviceName(deviceName) {
}

void BleMouseWrapper::clearBondingData() {
    // Remove all bonded BLE devices to fix pairing issues
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t* dev_list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (dev_list) {
            esp_ble_get_bond_device_list(&dev_num, dev_list);
            for (int i = 0; i < dev_num; i++) {
                esp_ble_remove_bond_device(dev_list[i].bd_addr);
            }
            free(dev_list);
        }
    }
}

void BleMouseWrapper::begin() {
    _bleMouse.begin();
}

void BleMouseWrapper::end() {
    _bleMouse.end();
}

bool BleMouseWrapper::isConnected() {
    return _bleMouse.isConnected();
}

void BleMouseWrapper::press(uint8_t button) {
    _bleMouse.press(button);
}

void BleMouseWrapper::release(uint8_t button) {
    _bleMouse.release(button);
}

void BleMouseWrapper::move(int x, int y) {
    _bleMouse.move(x, y);
}

const std::string& BleMouseWrapper::getDeviceName() const {
    return _deviceName;
}