#include "ble_mouse_wrapper.h"

BleMouseWrapper::BleMouseWrapper(const std::string& deviceName)
    : _bleMouse(deviceName, "M5Stack", 100), _deviceName(deviceName) {
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